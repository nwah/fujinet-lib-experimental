#include <fujinet-network.h>

#if !defined(__ADAM__) && !defined(__COLECOADAM__)
/* network_json_query() with a bounded buffer: stores at most buflen - 1
 * bytes plus a NUL, reading and discarding the rest of the value. */
int16_t network_json_query_n(const char *devicespec, const char *query, char *buffer, uint16_t buflen)
{
  static char discard[64];
  uint16_t total, avail, room, n;
  int16_t read_len;
  FN_ERR err;
  uint8_t nw_unit = network_unit(devicespec);

  if (!buflen)
    return -FN_ERR_BAD_CMD;

  if (!NETCALL_D(NETCMD_QUERY, nw_unit, query, MAX_JSON_QUERY_LEN))
    return -FN_ERR_IO_ERROR;

  total = 0;
  for (;;) {
    err = network_unit_status(nw_unit, &nw_status);
    if (err)
      return -err;

    avail = nw_status.avail;
    if (!avail)
      break;

    room = buflen - 1 - total;
    if (room) {
      n = avail < room ? avail : room;
      read_len = network_read(devicespec, &buffer[total], n);
      if (read_len < 0)
        return read_len;
      total += read_len;
    }
    else {
      n = avail < sizeof(discard) ? avail : sizeof(discard);
      read_len = network_read(devicespec, discard, n);
      if (read_len < 0)
        return read_len;
    }
  }

  return network_json_strip_newlines(buffer, total);
}
#endif /* ! (__ADAM__ || __COLECOADAM__) */
