/* bus/palmos/link_palmos.c
 *
 * Palm OS side of the FujiNet link: owns the FnSerPort/FnTransport/FnCtx
 * globals and the "FujiNet link" preference (see include/fujinet-palmos.h).
 * This is the one file besides transport_ser.c that includes PalmOS.h;
 * fujinet-bus-palmos.c stays portable and never touches Palm headers.
 *
 * Static globals are fine here: a program using this library is one Palm OS
 * process with a single open link, same as any other app-global state. The
 * library must not be called from a launch code that runs without globals
 * (e.g. sysAppLaunchCmdFind).
 */
/* fujinet-palmos.h (via fujinet-int.h) must be included before <PalmOS.h>:
 * it pulls in the compiler's <stdbool.h>, which declares true/false as
 * enumerators of its own `bool' enum. PalmTypes.h only declares its own
 * anonymous `enum {false, true}' if `true' isn't already a macro, so
 * loading stdbool.h first makes the two definitions coexist instead of
 * conflicting (both define the same two enumerator names otherwise). */
#include "fujinet-palmos.h"
#include "fujinet-bus-palmos.h"

#include <PalmOS.h>

#include "transport_ser.h"

static FnSerPort   s_port;
static FnTransport s_transport;
static FnCtx       s_ctx;
static Boolean     s_open = false;
static UInt16      s_last_error = 0;

void fuji_palmos_set_last_error(uint16_t err)
{
  s_last_error = (UInt16) err;
}

uint16_t fuji_palmos_last_error(void)
{
  return s_last_error;
}

bool fuji_palmos_is_open(void)
{
  return s_open ? true : false;
}

void fuji_palmos_close(void)
{
  if (s_open) {
    fn_ser_close(&s_port);
    s_open = false;
  }
}

bool fuji_palmos_open(const char *lib_name, uint32_t baud)
{
  Err err;

  fuji_palmos_close();

  err = fn_ser_open(&s_port, lib_name, (UInt32) baud);
  if (err != errNone) {
    s_last_error = (UInt16) err;
    return false;
  }

  fn_ser_transport(&s_port, &s_transport);
  fn_init(&s_ctx, &s_transport);
  s_open = true;
  s_last_error = 0;
  return true;
}

/* Lazily opens the link the first time fuji_bus_call() is used without an
 * explicit fuji_palmos_open(): reads the "FujiNet link" unsaved preference
 * and falls back to FUJI_PALMOS_DEFAULT_LIB/FUJI_PALMOS_DEFAULT_BAUD if it
 * is missing, the wrong version, or too short. */
static bool fuji_palmos_open_default(void)
{
  FujiPalmosLinkPref pref;
  UInt16 size;
  UInt16 version;
  const char *lib_name = FUJI_PALMOS_DEFAULT_LIB;
  UInt32 baud = FUJI_PALMOS_DEFAULT_BAUD;

  MemSet(&pref, sizeof(pref), 0);
  size = sizeof(pref);
  version = PrefGetAppPreferences(FUJI_PALMOS_PREF_CREATOR, FUJI_PALMOS_PREF_ID,
                                   &pref, &size, false);
  if (version == FUJI_PALMOS_PREF_VERSION && size >= sizeof(pref)) {
    pref.lib_name[sizeof(pref.lib_name) - 1] = '\0';
    lib_name = pref.lib_name;
    baud = pref.baud;
  }

  return fuji_palmos_open(lib_name, baud);
}

FnCtx *fuji_palmos_ctx(void)
{
  if (!s_open) {
    if (!fuji_palmos_open_default())
      return NULL;
  }
  return &s_ctx;
}
