/* bus/palmos/appkey.c
 *
 * fuji_bus_appkey_read/write (declared in include/fujinet-appkey.h for the
 * !_CMOC_VERSION_ case, implemented per bus/platform). Like
 * bus/msdos/appkey.c: the firmware's FUJICMD_READ_APPKEY reply is a
 * little-endian uint16 length followed by the key's bytes (a key that was
 * never written comes back as length 64 and zeros), so the header is read
 * into our own buffer and stripped. The length is decoded byte by byte
 * because the m68k is big-endian, and bounded by the reply length FujiBus
 * reports (fuji_bus_call_rlen, set in fujinet-bus-palmos.c).
 *
 * The appkey bytes themselves are opaque application data, not multi-byte
 * integers, so nothing here needs the endian fix-up table in
 * fujinet-bus-palmos.c.
 */
#include <string.h>
#include "fujinet-const.h"
#include "fujinet-int.h"
#include "fujinet-bus.h"
#include "fujinet-commands.h"

extern uint16_t fuji_bus_call_rlen;

static uint8_t appkey_buf[2 + MAX_APPKEY_LEN];

bool fuji_bus_appkey_read(void *string, uint16_t *length)
{
  uint16_t len;

  if (!FUJICALL_RV(FUJICMD_READ_APPKEY, appkey_buf, sizeof(appkey_buf)))
    return false;
  if (fuji_bus_call_rlen < 2)
    return false;

  len = appkey_buf[0] | ((uint16_t) appkey_buf[1] << 8);
  if (len > fuji_bus_call_rlen - 2)
    len = fuji_bus_call_rlen - 2;
  if (len > MAX_APPKEY_LEN)
    len = MAX_APPKEY_LEN;

  memmove(string, appkey_buf + 2, len);
  *length = len;
  return true;
}

bool fuji_bus_appkey_write(const void *string, uint16_t length)
{
  return FUJICALL_D(FUJICMD_WRITE_APPKEY, string, length);
}
