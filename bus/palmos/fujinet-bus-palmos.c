/* bus/palmos/fujinet-bus-palmos.c
 *
 * fuji_bus_call/network_bus_read/network_bus_write for Palm OS, built on top
 * of the portable FujiBus client (fujibus.c, vendored from fujinet-palm).
 *
 * Deliberately portable: this file must NOT include any Palm OS header, so
 * it can also be compiled (and its endian fix-up table unit tested) on a
 * plain host compiler. The one PalmOS-touching piece of the link -- opening
 * the serial port and holding the FnCtx -- lives in link_palmos.c behind
 * fuji_palmos_ctx() (declared in fujinet-bus-palmos.h).
 */
#include <string.h>

#include "fujinet-bus-palmos.h"
#include "fujinet-commands.h"

/* ------------------------------------------------------------------------
 * Endian fix-up table.
 *
 * FujiBus request/reply payload integers are little-endian on the wire (the
 * ESP32/fujinet-pc firmware serializes its structs raw off an LE machine).
 * m68k Palm OS is big-endian, so any multi-byte integer living *inside* a
 * DATA or REPLY payload buffer needs converting in place.
 *
 * Values packed into the aux1..aux4 bytes by the ez macros
 * (fujinet-bus-ezfixed.h, via NATIVE_SPLIT_U16/NATIVE_SPLIT_U32 in
 * fujinet-endian.h) are already produced in wire order arithmetically
 * (shift + mask, not a memory cast) and need no fixing here on any host
 * endianness -- this table only covers integers embedded in a payload
 * buffer that common/ code (or an app, via the fujinet-fuji.h /
 * fujinet-qrcode.h macros) reads or writes as a native C integer.
 *
 * Audited call sites that DO need a fix-up:
 *   1. common/network_status.c, network_read_nb.c, network_json_query.c
 *      -> network_unit_status()/NETCALL_RV(NETCMD_STATUS) reply, against a
 *         network device (FUJI_DEVICEID_NETWORK..NETWORK_LAST).
 *         include/fujinet-network.h: NetworkStatus.avail, offset 0, u16.
 *   2. include/fujinet-fuji.h fuji_get_directory_position()
 *      -> FUJICALL_RV(FUJICMD_GET_DIRECTORY_POSITION) reply, Fuji device.
 *         uint16_t position, offset 0, u16.
 *   3. include/fujinet-fuji.h fuji_base64_decode_length()
 *      -> FUJICALL_RV(FUJICMD_BASE64_DECODE_LENGTH) reply, Fuji device.
 *         unsigned long length, offset 0, u32.
 *   4. include/fujinet-fuji.h fuji_base64_encode_length()
 *      -> FUJICALL_RV(FUJICMD_BASE64_ENCODE_LENGTH) reply, Fuji device.
 *         unsigned long length, offset 0, u32.
 *   5. include/fujinet-qrcode.h fuji_qrcode_length()
 *      -> FUJICALL_A1_RV(FUJICMD_QRCODE_LENGTH) reply, Fuji device.
 *         unsigned long length, offset 0, u32.
 *   6. common/fuji_appkey.c init_appkey() / FUJICALL_D(FUJICMD_OPEN_APPKEY,
 *      &appkey, sizeof(appkey)) request DATA payload, Fuji device.
 *      FNAppKeyID.creator (uint16_t), offset 0, u16.
 *
 * Audited and found to need NO fix-up:
 *   - AdapterConfig / AdapterConfigExtended (fujinet-fuji.h): every field is
 *     a char[] or uint8_t[] byte array, nothing wider than a byte.
 *   - DeviceSlot / HostSlot (fujinet-fuji.h): same, byte arrays only.
 *   - fuji_generate_guid, fuji_get_host_prefix, fuji_get_device_filename,
 *     fuji_bus_appkey_read/write key bytes (bus/palmos/appkey.c): opaque
 *     strings/binary blobs, not interpreted as integers by the library.
 *   - network_seek() (NETCALL_C1234), fuji_set_directory_position() (B12),
 *     fuji_base64_*_input()/fuji_qrcode_input()/fuji_hash_input() (B12):
 *     values packed into aux1..aux4 via NATIVE_SPLIT_U16/U32, already wire
 *     order on any host endianness (see comment above).
 *   - network_tell_common.c: already decodes its 4-byte LE reply manually,
 *     byte by byte, into a uint32_t -- never casts the reply buffer.
 *   - fuji_hash_length()/fuji_hash_output(): despite the macro's "len"
 *     parameter name, every caller in common/ (fuji_hash.c,
 *     fuji_hash_calculate.c) passes a 1-byte uint8_t reply, not the 4-byte
 *     "unsigned long" the doc comment describes as deprecated/broken.
 *   - fuji_get_time() (7 raw calendar-field bytes, each 1 byte) and
 *     clock_get_time_common()/clock_get_time_tz() (fuji_clock.c): every
 *     TimeFormat layout in clk_reply_len[] is a sequence of 1-byte fields
 *     or a NUL-terminated ISO/SOS string, EXCEPT PRODOS_BINARY (4 bytes),
 *     which is an opaque bit-packed ProDOS date/time blob the calling app
 *     decodes itself -- not fixed here, see report.
 * ------------------------------------------------------------------------ */

typedef struct {
  uint8_t dev_lo, dev_hi; /* inclusive device id range */
  uint8_t cmd;
  uint8_t is_data;        /* 0 = fix up the REPLY buffer, 1 = fix up the DATA buffer */
  uint8_t offset;
  uint8_t width;          /* 2 or 4 */
} PalmosSwapField;

static const PalmosSwapField palmos_swap_table[] = {
  { FUJI_DEVICEID_NETWORK, FUJI_DEVICEID_NETWORK_LAST, NETCMD_STATUS,                 0, 0, 2 },
  { FUJI_DEVICEID_FUJINET, FUJI_DEVICEID_FUJINET,      FUJICMD_GET_DIRECTORY_POSITION, 0, 0, 2 },
  { FUJI_DEVICEID_FUJINET, FUJI_DEVICEID_FUJINET,      FUJICMD_BASE64_DECODE_LENGTH,   0, 0, 4 },
  { FUJI_DEVICEID_FUJINET, FUJI_DEVICEID_FUJINET,      FUJICMD_BASE64_ENCODE_LENGTH,   0, 0, 4 },
  { FUJI_DEVICEID_FUJINET, FUJI_DEVICEID_FUJINET,      FUJICMD_QRCODE_LENGTH,          0, 0, 4 },
  { FUJI_DEVICEID_FUJINET, FUJI_DEVICEID_FUJINET,      FUJICMD_OPEN_APPKEY,            1, 0, 2 },
};
#define PALMOS_SWAP_TABLE_LEN (sizeof(palmos_swap_table) / sizeof(palmos_swap_table[0]))

/* Decodes `width` little-endian wire bytes at p into a native uint32_t.
 * Not static: fujinet-palm's host/fnlib_host.c unit tests its value semantics
 * directly (see that file), since a whole-buffer round trip through
 * fuji_bus_call can't distinguish "no swap needed" from "swap works" on a
 * little-endian host. */
uint32_t palmos_decode_le(const uint8_t *p, uint8_t width)
{
  uint32_t v = 0;
  uint8_t i;

  for (i = 0; i < width; i++)
    v |= ((uint32_t) p[i]) << (8 * i);
  return v;
}

/* Encodes v as `width` little-endian wire bytes at p. Not static, see
 * palmos_decode_le. */
void palmos_encode_le(uint8_t *p, uint32_t v, uint8_t width)
{
  uint8_t i;

  for (i = 0; i < width; i++)
    p[i] = (uint8_t) (v >> (8 * i));
}

/* Reply direction: buf[offset..offset+width) holds little-endian wire bytes;
 * rewrite them as the native in-memory representation of the same value.
 * Correct on both endiannesses: on a little-endian host decode+store is the
 * identity transform; on a big-endian host (Palm) it produces the value's
 * true big-endian in-memory layout. */
static void palmos_fixup_reply_field(uint8_t *buf, uint8_t offset, uint8_t width)
{
  uint32_t v = palmos_decode_le(&buf[offset], width);

  if (width == 2) {
    uint16_t v16 = (uint16_t) v;
    memcpy(&buf[offset], &v16, sizeof(v16));
  } else {
    memcpy(&buf[offset], &v, sizeof(v));
  }
}

/* Data (request) direction: buf[offset..offset+width) holds a native
 * in-memory integer; rewrite it as little-endian wire bytes. Also correct
 * on both endiannesses for the same reason as above, run in reverse. */
static void palmos_fixup_data_field(uint8_t *buf, uint8_t offset, uint8_t width)
{
  uint32_t v;

  if (width == 2) {
    uint16_t v16;
    memcpy(&v16, &buf[offset], sizeof(v16));
    v = v16;
  } else {
    memcpy(&v, &buf[offset], sizeof(v));
  }
  palmos_encode_le(&buf[offset], v, width);
}

/* Applies every REPLY-direction table entry matching (device, cmd) to a
 * reply buffer of `len` bytes (skips any field that would run past len). */
static void palmos_fixup_reply(uint8_t device, uint8_t cmd, uint8_t *buf, uint16_t len)
{
  uint8_t i;

  for (i = 0; i < PALMOS_SWAP_TABLE_LEN; i++) {
    const PalmosSwapField *f = &palmos_swap_table[i];

    if (f->is_data || device < f->dev_lo || device > f->dev_hi || cmd != f->cmd)
      continue;
    if ((uint16_t) f->offset + f->width > len)
      continue;
    palmos_fixup_reply_field(buf, f->offset, f->width);
  }
}

/* Applies every DATA-direction table entry matching (device, cmd) to a
 * request payload buffer of `len` bytes before it is sent. */
static void palmos_fixup_data(uint8_t device, uint8_t cmd, uint8_t *buf, uint16_t len)
{
  uint8_t i;

  for (i = 0; i < PALMOS_SWAP_TABLE_LEN; i++) {
    const PalmosSwapField *f = &palmos_swap_table[i];

    if (!f->is_data || device < f->dev_lo || device > f->dev_hi || cmd != f->cmd)
      continue;
    if ((uint16_t) f->offset + f->width > len)
      continue;
    palmos_fixup_data_field(buf, f->offset, f->width);
  }
}

/* Staging buffer for outgoing DATA payloads: request fix-ups are applied to
 * a copy, never to the caller's const buffer. One buffer is enough because
 * fuji_bus_call is not reentrant (single link, single outstanding call). */
static uint8_t palmos_data_staging[FN_MAX_DATA];

/* The exact reply payload length of the last fuji_bus_call(), as reported
 * by fn_bus_call() itself (not a fixed block size). bus/palmos/appkey.c
 * uses it to bound the length header in FUJICMD_READ_APPKEY's reply.
 * Mirrors the (undeclared, file-local-by-convention) global of the same
 * name in bus/msdos/fujinet-bus-msdos.c. */
uint16_t fuji_bus_call_rlen;

bool fuji_bus_call(uint8_t device, uint8_t fuji_cmd, uint8_t fields,
                   uint8_t aux1, uint8_t aux2, uint8_t aux3, uint8_t aux4,
                   const void *buf, size_t buf_length)
{
  FnCtx *ctx;
  FnParams params;
  const void *data_ptr = NULL;
  uint16_t data_len = 0;
  void *reply_ptr = NULL;
  uint16_t reply_max = 0;
  uint16_t reply_len = 0;
  FnErr err;

  ctx = fuji_palmos_ctx();
  if (!ctx)
    return false;

  fn_params_none(&params);
  switch (fields & 0x07) {
    case 1:
      fn_params_add_u8(&params, aux1);
      break;
    case 2:
      fn_params_add_u8(&params, aux1);
      fn_params_add_u8(&params, aux2);
      break;
    case 3:
      fn_params_add_u8(&params, aux1);
      fn_params_add_u8(&params, aux2);
      fn_params_add_u8(&params, aux3);
      break;
    case 4:
      fn_params_add_u8(&params, aux1);
      fn_params_add_u8(&params, aux2);
      fn_params_add_u8(&params, aux3);
      fn_params_add_u8(&params, aux4);
      break;
    case 5:
      fn_params_add_u16(&params, (uint16_t) (aux1 | ((uint16_t) aux2 << 8)));
      break;
    case 6:
      fn_params_add_u16(&params, (uint16_t) (aux1 | ((uint16_t) aux2 << 8)));
      fn_params_add_u16(&params, (uint16_t) (aux3 | ((uint16_t) aux4 << 8)));
      break;
    case 7:
      fn_params_add_u32(&params, (uint32_t) aux1 | ((uint32_t) aux2 << 8)
                         | ((uint32_t) aux3 << 16) | ((uint32_t) aux4 << 24));
      break;
    default:
      break;
  }

  if (fields & FUJI_FIELD_DATA) {
    if (buf_length > FN_MAX_DATA)
      return false;
    if (buf_length > 0) {
      memcpy(palmos_data_staging, buf, buf_length);
      palmos_fixup_data(device, fuji_cmd, palmos_data_staging, (uint16_t) buf_length);
      data_ptr = palmos_data_staging;
      data_len = (uint16_t) buf_length;
    }
  } else if (fields & FUJI_FIELD_REPLY) {
    reply_ptr = (void *) buf;
    reply_max = (uint16_t) buf_length;
  }

  err = fn_bus_call(ctx, device, fuji_cmd, &params, data_ptr, data_len,
                     reply_ptr, reply_max, &reply_len);
  fuji_palmos_set_last_error((uint16_t) err);
  if (err != FN_OK)
    return false;

  fuji_bus_call_rlen = reply_len;

  if (reply_ptr != NULL && reply_len > 0) {
    uint16_t copy_len = reply_len < reply_max ? reply_len : reply_max;
    palmos_fixup_reply(device, fuji_cmd, (uint8_t *) reply_ptr, copy_len);
  }

  return true;
}

/* NETCALL_B12_RV/D's data/reply payload rides in one FujiBus frame, capped
 * at FN_MAX_DATA (512, see fujibus.h) -- but a NetworkStatus.avail bytes-
 * waiting count (what network_read_nb.c bases its request length on) can
 * legitimately be larger than that for a sizeable HTTP response. A single
 * NETCALL_B12_RV/D for more than FN_MAX_DATA bytes overflows FnCtx::rx and
 * comes back as FN_ERR_LENGTH (confirmed against a live fujinet-pc: a 559-
 * byte read silently failed before this loop was added). Chunk to
 * FN_MAX_DATA-sized FujiBus transactions transparently; this is a wire
 * protocol limit, not a host-test artifact, so it applies on real Palm OS
 * hardware too. */
size_t network_bus_read(uint8_t device, void *buffer, size_t length)
{
  uint8_t *p = (uint8_t *) buffer;
  size_t total = 0;
  size_t chunk;

  while (total < length) {
    chunk = length - total;
    if (chunk > FN_MAX_DATA)
      chunk = FN_MAX_DATA;
    if (!NETCALL_B12_RV(NETCMD_READ, device - FUJI_DEVICEID_NETWORK + 1,
                        chunk, p + total, chunk))
      break;
    total += chunk;
  }
  return total;
}

size_t network_bus_write(uint8_t device, const void *buffer, size_t length)
{
  const uint8_t *p = (const uint8_t *) buffer;
  size_t total = 0;
  size_t chunk;

  while (total < length) {
    chunk = length - total;
    if (chunk > FN_MAX_DATA)
      chunk = FN_MAX_DATA;
    if (!NETCALL_B12_D(NETCMD_WRITE, device - FUJI_DEVICEID_NETWORK + 1,
                       chunk, p + total, chunk))
      break;
    total += chunk;
  }
  return total;
}
