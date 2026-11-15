#ifndef ON1X_STDLIB_MULIB_BRIDGE_H
#define ON1X_STDLIB_MULIB_BRIDGE_H
#include <stdbool.h>
#include <stdint.h>
/* Raw scalar ABI. Status belongs to the most recent bridge operation on this
 * single-threaded adapter; read immediately, before another operation. */
enum { ON1X_BRIDGE_OK=0, ON1X_BRIDGE_MISSING=1, ON1X_BRIDGE_EOF=2,
       ON1X_BRIDGE_AGAIN=3, ON1X_BRIDGE_INVALID=-1, ON1X_BRIDGE_PARSE=-2,
       ON1X_BRIDGE_MEMORY=-3, ON1X_BRIDGE_LIMIT=-4, ON1X_BRIDGE_IO=-5,
       ON1X_BRIDGE_BINARY=-6 };
int64_t on1x_mulib_bridge_status(void);
bool on1x_mulib_json_is_valid(const char *);
const char *on1x_mulib_json_normalize(const char *);
int64_t on1x_mulib_json_array_length(const char *);
const char *on1x_mulib_yaml_to_json(const char *);
const char *on1x_mulib_toml_to_json(const char *);
bool on1x_mulib_ini_has(const char *,const char *,const char *);
const char *on1x_mulib_ini_get(const char *,const char *,const char *);
const char *on1x_mulib_blake3_hex(const char *);
const char *on1x_mulib_blake2s_hex(const char *);
int64_t on1x_mulib_unix_getpid(void);
int64_t on1x_mulib_unix_getuid(void);
int64_t on1x_mulib_socket_connect_ipv4(const char *,int64_t);
int64_t on1x_mulib_socket_send(int64_t,const char *);
int64_t on1x_mulib_socket_send_from(int64_t,const char *,int64_t);
const char *on1x_mulib_socket_recv(int64_t,int64_t);
const char *on1x_mulib_socket_recv_hex(int64_t,int64_t);
bool on1x_mulib_socket_close(int64_t);
#endif
