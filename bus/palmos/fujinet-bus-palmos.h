#ifndef FUJINET_BUS_PALMOS_H
#define FUJINET_BUS_PALMOS_H

#include "fujinet-bus.h"
#include "fujibus.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Returns the current FujiNet link's FnCtx, opening it lazily (from the
 * "FujiNet link" preference, or the built-in defaults, see
 * include/fujinet-palmos.h) on first use if the app never called
 * fuji_palmos_open() itself. Returns NULL if no link could be opened --
 * fuji_palmos_last_error() then holds the reason.
 *
 * Implemented in link_palmos.c (the only other file besides
 * transport_ser.c that touches PalmOS.h), so that fujinet-bus-palmos.c
 * itself stays portable and can be compiled and unit tested on a host
 * machine too. */
FnCtx *fuji_palmos_ctx(void);

/* Records the last FnErr (from fn_bus_call, in fujinet-bus-palmos.c) or
 * Palm OS Err (from fn_ser_open, in link_palmos.c) for
 * fuji_palmos_last_error(). Implemented in link_palmos.c. */
void fuji_palmos_set_last_error(uint16_t err);

#ifdef __cplusplus
}
#endif

#endif /* FUJINET_BUS_PALMOS_H */
