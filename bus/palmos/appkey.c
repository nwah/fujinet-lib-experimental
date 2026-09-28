/* bus/palmos/appkey.c
 *
 * fuji_bus_appkey_read/write (declared in include/fujinet-appkey.h for the
 * !_CMOC_VERSION_ case, implemented per bus/platform). Modeled on
 * bus/msdos/appkey.c, but simpler: FujiBus always reports the exact reply
 * payload length via fn_bus_call()'s reply_len out-param (surfaced here as
 * fuji_bus_call_rlen, set by fuji_bus_call() in fujinet-bus-palmos.c), so
 * there's no need for msdos's length-prefixed-reply workaround for a
 * transport that can't tell a short reply from a padded one.
 *
 * The appkey bytes themselves are opaque application data, not multi-byte
 * integers, so nothing here needs the endian fix-up table in
 * fujinet-bus-palmos.c.
 */
#include "fujinet-const.h"
#include "fujinet-int.h"
#include "fujinet-bus.h"
#include "fujinet-commands.h"

extern uint16_t fuji_bus_call_rlen;

bool fuji_bus_appkey_read(void *string, uint16_t *length)
{
  if (!FUJICALL_RV(FUJICMD_READ_APPKEY, string, MAX_APPKEY_LEN))
    return false;
  *length = fuji_bus_call_rlen;
  return true;
}

bool fuji_bus_appkey_write(const void *string, uint16_t length)
{
  return FUJICALL_D(FUJICMD_WRITE_APPKEY, string, length);
}
