#ifndef FUJINET_PALMOS_H
#define FUJINET_PALMOS_H

#include <fujinet-int.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Which Serial Manager library the FujiNet link uses, and its baud.
   If an app never calls fuji_palmos_open(), the first fuji_bus_call() opens
   the link from the "FujiNet link" preference (see below), else the defaults. */
bool fuji_palmos_open(const char *lib_name, uint32_t baud);
void fuji_palmos_close(void);   /* apps MUST call this in AppStop so HotSync etc. can use the port */
bool fuji_palmos_is_open(void);
uint16_t fuji_palmos_last_error(void); /* last FnErr or Palm Err, for diagnostics */

#define FUJI_PALMOS_PREF_CREATOR 'FNCF'
#define FUJI_PALMOS_PREF_ID 0
#define FUJI_PALMOS_PREF_VERSION 1

typedef struct {
  uint16_t version;
  uint32_t baud;
  char lib_name[32];
} FujiPalmosLinkPref;

#define FUJI_PALMOS_DEFAULT_LIB "Serial Library"
#define FUJI_PALMOS_DEFAULT_BAUD 115200UL

#ifdef __cplusplus
}
#endif

#endif /* FUJINET_PALMOS_H */
