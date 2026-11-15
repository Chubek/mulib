/* Raw, single-threaded scalar adapters. Implementation switches belong ONLY to
 * the planner's kernel TU, never this file. Enable selected ON1X_BRIDGE_* groups. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "mulib_bridge.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#ifndef ON1X_BRIDGE_MALLOC
#define ON1X_BRIDGE_MALLOC malloc
#endif
#ifndef ON1X_BRIDGE_POOL_BYTES
#define ON1X_BRIDGE_POOL_BYTES (32u*1024u*1024u)
#endif
#define BRIDGE_INPUT_BYTES (2u*1024u*1024u)
#define BRIDGE_TEXT_BYTES (4u*1024u*1024u)
static int64_t status;
int64_t on1x_mulib_bridge_status(void) { return status; }
#if defined(ON1X_BRIDGE_JSON) || defined(ON1X_BRIDGE_YAML) || defined(ON1X_BRIDGE_TOML) || defined(ON1X_BRIDGE_INI) || defined(ON1X_BRIDGE_CRYPTO) || defined(ON1X_BRIDGE_SOCKET) || defined(ON1X_BRIDGE_UNIX)
static bool input(const char *text) {
  status=ON1X_BRIDGE_OK;
  if(!text) { status=ON1X_BRIDGE_INVALID;return false; }
  if(strlen(text)>BRIDGE_INPUT_BYTES) { status=ON1X_BRIDGE_LIMIT;return false; }
  return true;
}
#endif
#if defined(ON1X_BRIDGE_JSON) || defined(ON1X_BRIDGE_YAML) || defined(ON1X_BRIDGE_TOML) || defined(ON1X_BRIDGE_INI) || defined(ON1X_BRIDGE_CRYPTO) || defined(ON1X_BRIDGE_SOCKET)
struct saved { struct saved *next; char value[]; };
static struct saved *saved_values;
static size_t saved_bytes;
static bool cleanup_registered;
static void cleanup(void) {
  while(saved_values) { struct saved *x=saved_values;saved_values=x->next;free(x); }
}
static char *reserve(size_t n) {
  if(n>BRIDGE_TEXT_BYTES || n>SIZE_MAX-sizeof(struct saved)-1) { status=ON1X_BRIDGE_LIMIT;return NULL; }
  size_t cost=sizeof(struct saved)+n+1;
  if(cost>ON1X_BRIDGE_POOL_BYTES-saved_bytes) { status=ON1X_BRIDGE_LIMIT;return NULL; }
  if(!cleanup_registered) {
    if(atexit(cleanup)) { status=ON1X_BRIDGE_MEMORY;return NULL; }
    cleanup_registered=true;
  }
  struct saved *p=ON1X_BRIDGE_MALLOC(cost);
  if(!p) { status=ON1X_BRIDGE_MEMORY;return NULL; }
  saved_bytes+=cost;p->next=saved_values;saved_values=p;p->value[n]=0;
  return p->value;
}
#endif
#if defined(ON1X_BRIDGE_JSON) || defined(ON1X_BRIDGE_YAML) || defined(ON1X_BRIDGE_TOML)
#include "mulib-json.h"
static const char *serialize(mulib_json_t *value) {
  if(!value) { status=ON1X_BRIDGE_PARSE;return ""; }
  char *text=mulib_json_stringify(value,0);
  mulib_json_destroy(value);
  if(!text) { status=ON1X_BRIDGE_MEMORY;return ""; }
  size_t n=strlen(text);char *out=reserve(n);
  if(out)memcpy(out,text,n);
  free(text);return out?out:"";
}
#endif
#ifdef ON1X_BRIDGE_JSON
bool on1x_mulib_json_is_valid(const char *text) {
  if(!input(text))return false;
  mulib_json_t *value=mulib_json_parse(text,strlen(text),NULL);
  if(!value) { status=ON1X_BRIDGE_PARSE;return false; }
  mulib_json_destroy(value);return true;
}
const char *on1x_mulib_json_normalize(const char *text) {
  if(!input(text))return "";
  return serialize(mulib_json_parse(text,strlen(text),NULL));
}
int64_t on1x_mulib_json_array_length(const char *text) {
  if(!input(text))return -1;
  mulib_json_t *value=mulib_json_parse(text,strlen(text),NULL);
  if(!value) { status=ON1X_BRIDGE_PARSE;return -1; }
  int64_t count=-1;
  if(mulib_json_typeof(value)==MULIB_JSON_ARRAY)count=(int64_t)mulib_json_size(value);
  else status=ON1X_BRIDGE_INVALID;
  mulib_json_destroy(value);return count;
}
#endif
#ifdef ON1X_BRIDGE_YAML
#include "mulib-yaml.h"
const char *on1x_mulib_yaml_to_json(const char *text) {
  if(!input(text))return "";
  return serialize(mulib_yaml_parse(text,strlen(text),NULL));
}
#endif
#ifdef ON1X_BRIDGE_TOML
#include "mulib-toml.h"
const char *on1x_mulib_toml_to_json(const char *text) {
  if(!input(text))return "";
  return serialize(mulib_toml_parse(text,strlen(text),NULL));
}
#endif
#ifdef ON1X_BRIDGE_INI
#include "mulib-ini.h"
struct ini_search { const char *section,*key;char *value;bool found,copy; };
static int ini_callback(void *context,const char *section,const char *key,const char *value) {
  struct ini_search *s=context;
  if(s->found || strcmp(s->section,section) || strcmp(s->key,key))return 1;
  s->found=true;
  if(s->copy) {
    size_t n=strlen(value);s->value=ON1X_BRIDGE_MALLOC(n+1);
    if(!s->value) { status=ON1X_BRIDGE_MEMORY;return 0; }
    memcpy(s->value,value,n+1);
  }
  return 1; /* Validate the entire document, not just the prefix before a match. */
}
static const char *ini_lookup(const char *text,const char *section,const char *key,bool copy) {
  if(!input(text) || !input(section) || !input(key))return "";
  struct ini_search s={section,key,NULL,false,copy};
  int ok=mulib_ini_parse(text,ini_callback,&s);
  if(!ok) { free(s.value);if(status==0)status=ON1X_BRIDGE_PARSE;return ""; }
  if(!s.found) { status=ON1X_BRIDGE_MISSING;return ""; }
  if(!copy)return "";
  size_t n=strlen(s.value);char *out=reserve(n);
  if(out)memcpy(out,s.value,n);
  free(s.value);return out?out:"";
}
bool on1x_mulib_ini_has(const char *text,const char *section,const char *key) {
  (void)ini_lookup(text,section,key,false);return status==0;
}
const char *on1x_mulib_ini_get(const char *text,const char *section,const char *key) {
  return ini_lookup(text,section,key,true);
}
#endif
#if defined(ON1X_BRIDGE_CRYPTO) || defined(ON1X_BRIDGE_SOCKET)
static const char *hex_text(const unsigned char *bytes,size_t n) {
  static const char alphabet[]="0123456789abcdef";
  if(n>BRIDGE_TEXT_BYTES/2) { status=ON1X_BRIDGE_LIMIT;return ""; }
  char *out=reserve(2*n);if(!out)return "";
  for(size_t i=0;i<n;++i) { out[2*i]=alphabet[bytes[i]>>4];out[2*i+1]=alphabet[bytes[i]&15]; }
  return out;
}
#endif
#ifdef ON1X_BRIDGE_CRYPTO
#include "mulib-cryptohash.h"
const char *on1x_mulib_blake3_hex(const char *text) {
  unsigned char digest[32];if(!input(text))return "";
  if(!mulib_blake3(text,strlen(text),digest,sizeof digest)) { status=ON1X_BRIDGE_MEMORY;return ""; }
  return hex_text(digest,sizeof digest);
}
const char *on1x_mulib_blake2s_hex(const char *text) {
  unsigned char digest[32];if(!input(text))return "";
  if(!mulib_blake2s(text,strlen(text),NULL,0,digest,sizeof digest)) { status=ON1X_BRIDGE_MEMORY;return ""; }
  return hex_text(digest,sizeof digest);
}
#endif
#ifdef ON1X_BRIDGE_UNIX
#include <unistd.h>
int64_t on1x_mulib_unix_getpid(void) { (void)input("");return (int64_t)getpid(); }
int64_t on1x_mulib_unix_getuid(void) { (void)input("");return (int64_t)getuid(); }
#endif
#ifdef ON1X_BRIDGE_SOCKET
#include "mulib-socket.h"
#define MAX_SOCKETS 64
#define SLOT_FACTOR 128ULL
static struct { mulib_socket_t socket;uint64_t generation;bool active; } slots[MAX_SOCKETS];
static bool sockets_initialized;
static void close_all(void) {
  for(size_t i=0;i<MAX_SOCKETS;++i)if(slots[i].active) { mulib_socket_close(&slots[i].socket);slots[i].active=false; }
}
static bool init_sockets(void) {
  if(sockets_initialized)return true;
  if(atexit(close_all)) { status=ON1X_BRIDGE_MEMORY;return false; }
  for(size_t i=0;i<MAX_SOCKETS;++i) { slots[i].socket.fd=-1;slots[i].generation=1; }
  sockets_initialized=true;return true;
}
static int slot_of(int64_t handle) {
  status=ON1X_BRIDGE_OK;
  if(handle>0) {
    uint64_t raw=(uint64_t)handle-1,index=raw%SLOT_FACTOR,generation=raw/SLOT_FACTOR;
    if(index<MAX_SOCKETS && slots[index].active && slots[index].generation==generation)return (int)index;
  }
  status=ON1X_BRIDGE_INVALID;return -1;
}
static void socket_status(int code) {
  status=code==MULIB_SOCKET_OK?ON1X_BRIDGE_OK:code==MULIB_SOCKET_EOF?ON1X_BRIDGE_EOF:
         code==MULIB_SOCKET_AGAIN?ON1X_BRIDGE_AGAIN:code==MULIB_SOCKET_EINVAL?ON1X_BRIDGE_INVALID:ON1X_BRIDGE_IO;
}
int64_t on1x_mulib_socket_connect_ipv4(const char *address,int64_t port) {
  if(!input(address))return -1;
  if(port<1 || port>65535) { status=ON1X_BRIDGE_INVALID;return -1; }
  if(!init_sockets())return -1;
  for(size_t i=0;i<MAX_SOCKETS;++i)if(!slots[i].active && slots[i].generation<=((uint64_t)INT64_MAX-i-1)/SLOT_FACTOR) {
    mulib_socket_t socket=MULIB_SOCKET_INIT;
    int code=mulib_socket_connect(&socket,address,(unsigned)port);socket_status(code);
    if(code!=MULIB_SOCKET_OK) { mulib_socket_close(&socket);return -1; }
    slots[i].socket=socket;slots[i].active=true;
    return (int64_t)(slots[i].generation*SLOT_FACTOR+i+1);
  }
  status=ON1X_BRIDGE_LIMIT;return -1;
}
int64_t on1x_mulib_socket_send_from(int64_t handle,const char *text,int64_t offset) {
  if(!input(text))return -1;
  int i=slot_of(handle);if(i<0)return -1;
  size_t n=strlen(text),sent=0;
  if(offset<0 || (uint64_t)offset>n) { status=ON1X_BRIDGE_INVALID;return -1; }
  int code=mulib_socket_send(&slots[i].socket,text+(size_t)offset,n-(size_t)offset,&sent);socket_status(code);
  return code==MULIB_SOCKET_OK?(int64_t)sent:-1;
}
int64_t on1x_mulib_socket_send(int64_t handle,const char *text) { return on1x_mulib_socket_send_from(handle,text,0); }
static const char *receive(int64_t handle,int64_t limit,bool hex) {
  int i=slot_of(handle);if(i<0)return "";
  if(limit<=0 || limit>1048576) { status=ON1X_BRIDGE_INVALID;return ""; }
  unsigned char *bytes=ON1X_BRIDGE_MALLOC((size_t)limit+1);
  if(!bytes) { status=ON1X_BRIDGE_MEMORY;return ""; }
  size_t received=0;int code=mulib_socket_receive(&slots[i].socket,bytes,(size_t)limit,&received);socket_status(code);
  if(code!=MULIB_SOCKET_OK) { free(bytes);return ""; }
  const char *result="";
  if(hex)result=hex_text(bytes,received);
  else if(memchr(bytes,0,received))status=ON1X_BRIDGE_BINARY;
  else { char *text=reserve(received);if(text) { memcpy(text,bytes,received);result=text; } }
  free(bytes);return result;
}
const char *on1x_mulib_socket_recv(int64_t handle,int64_t limit) { return receive(handle,limit,false); }
const char *on1x_mulib_socket_recv_hex(int64_t handle,int64_t limit) { return receive(handle,limit,true); }
bool on1x_mulib_socket_close(int64_t handle) {
  int i=slot_of(handle);if(i<0)return false;
  int code=mulib_socket_close(&slots[i].socket);socket_status(code);
  slots[i].active=false;++slots[i].generation;return code==MULIB_SOCKET_OK;
}
#endif
