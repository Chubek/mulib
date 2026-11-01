/**
 * @file mulib-virtio.h
 * @brief Virtio device registry, wire constants, and MuLIB VMM service contracts.
 * Define MULIB_IMPL_VIRTIO, MULIB_IMPL_VMMDEV and MULIB_IMPL_ENDIAN once.
 * Device IDs are identifiers, not claims of implementation. Query support().
 */
#ifndef MULIB_VIRTIO_H
#define MULIB_VIRTIO_H
#include "mulib-vmmdev.h"
#ifdef __cplusplus
extern "C" {
#endif
#define MULIB_VIRTIO_VERSION_1 (UINT64_C(1) << 32)
#define MULIB_VIRTIO_ACKNOWLEDGE 1u
#define MULIB_VIRTIO_DRIVER 2u
#define MULIB_VIRTIO_DRIVER_OK 4u
#define MULIB_VIRTIO_FEATURES_OK 8u
#define MULIB_VIRTIO_NEEDS_RESET 64u
#define MULIB_VIRTIO_FAILED 128u
#define MULIB_VIRTIO_CONFIG_MAX 256u
#define MULIB_VIRTIO_TRANSFER_MAX (1024u * 1024u)
enum mulib_virtio_id {
  MULIB_VIRTIO_NET = 1, MULIB_VIRTIO_BLK = 2, MULIB_VIRTIO_CONSOLE = 3,
  MULIB_VIRTIO_RNG = 4, MULIB_VIRTIO_BALLOON = 5, MULIB_VIRTIO_IOMEM = 6,
  MULIB_VIRTIO_RPMSG = 7, MULIB_VIRTIO_SCSI = 8, MULIB_VIRTIO_9P = 9,
  MULIB_VIRTIO_MAC80211_WLAN = 10, MULIB_VIRTIO_RPROC_SERIAL = 11,
  MULIB_VIRTIO_CAIF = 12, MULIB_VIRTIO_MEMORY_BALLOON = 13,
  MULIB_VIRTIO_GPU = 16, MULIB_VIRTIO_RTC = 17, MULIB_VIRTIO_INPUT = 18,
  MULIB_VIRTIO_VSOCK = 19, MULIB_VIRTIO_CRYPTO = 20,
  MULIB_VIRTIO_SIGNAL_DIST = 21, MULIB_VIRTIO_PSTORE = 22,
  MULIB_VIRTIO_IOMMU = 23, MULIB_VIRTIO_MEM = 24, MULIB_VIRTIO_SND = 25,
  MULIB_VIRTIO_FS = 26, MULIB_VIRTIO_PMEM = 27, MULIB_VIRTIO_RPMB = 28,
  MULIB_VIRTIO_HWSIM = 29, MULIB_VIRTIO_VIDEO_ENCODER = 30,
  MULIB_VIRTIO_VIDEO_DECODER = 31, MULIB_VIRTIO_SCMI = 32,
  MULIB_VIRTIO_NSM = 33, MULIB_VIRTIO_I2C = 34, MULIB_VIRTIO_WATCHDOG = 35,
  MULIB_VIRTIO_CAN = 36, MULIB_VIRTIO_DMABUF = 37,
  MULIB_VIRTIO_PARAM_SERV = 38, MULIB_VIRTIO_AUDIO_POLICY = 39,
  MULIB_VIRTIO_BT = 40, MULIB_VIRTIO_GPIO = 41, MULIB_VIRTIO_RDMA = 42,
  MULIB_VIRTIO_CAMERA = 43, MULIB_VIRTIO_ISM = 44, MULIB_VIRTIO_SPI = 45,
  MULIB_VIRTIO_TEE = 46, MULIB_VIRTIO_CPU_BALLOON = 47,
  MULIB_VIRTIO_MEDIA = 48, MULIB_VIRTIO_USB = 49, MULIB_VIRTIO_VIDEO = 50
};
enum mulib_virtio_support {
  MULIB_VIRTIO_PROFILE = 1, MULIB_VIRTIO_PENDING, MULIB_VIRTIO_SPEC_REQUIRED
};
/** @brief Immutable metadata; features exclude the common VERSION_1 bit. */
typedef struct mulib_virtio_profile {
  uint32_t id;
  const char *name;
  unsigned support, queues, config_size;
  uint64_t features, required_features;
} mulib_virtio_profile_t;
/** @brief Enumerated host services. Services implement host I/O, not transport parsing. */
enum mulib_virtio_service_op {
  MULIB_VIRTIO_SERVICE_RESET = 1,
  MULIB_VIRTIO_SERVICE_READ, MULIB_VIRTIO_SERVICE_WRITE,
  MULIB_VIRTIO_SERVICE_FLUSH, MULIB_VIRTIO_SERVICE_ID,
  MULIB_VIRTIO_SERVICE_RECEIVE, MULIB_VIRTIO_SERVICE_SEND,
  MULIB_VIRTIO_SERVICE_ENTROPY,
  MULIB_VIRTIO_SERVICE_INFLATE, MULIB_VIRTIO_SERVICE_DEFLATE,
  MULIB_VIRTIO_SERVICE_REQUEST, MULIB_VIRTIO_SERVICE_INPUT_CONFIG,
  MULIB_VIRTIO_SERVICE_GPIO, MULIB_VIRTIO_SERVICE_I2C,
  MULIB_VIRTIO_SERVICE_PLUG, MULIB_VIRTIO_SERVICE_UNPLUG,
  MULIB_VIRTIO_SERVICE_SCSI_COMMAND, MULIB_VIRTIO_SERVICE_SCSI_TMF,
  MULIB_VIRTIO_SERVICE_IOMMU, MULIB_VIRTIO_SERVICE_PCM_CONTROL,
  MULIB_VIRTIO_SERVICE_CIPHER, MULIB_VIRTIO_SERVICE_RPMB,
  MULIB_VIRTIO_SERVICE_GPU
};
/**
 * @brief Borrowed host-service call. address/flags interpretation is profile-specific.
 * Input is snapshotted. Output is private scratch; return the written length in used.
 * Return a MULIB_VMMDEV result. AGAIN requires no side effects. RESET must succeed.
 * No callback may retain pointers or reenter the endpoint. See the backend manual.
 */
typedef struct mulib_virtio_service_request {
  uint32_t device_id;
  unsigned operation, queue;
  uint64_t address;
  uint32_t flags;
  const unsigned char *input;
  size_t input_size;
  unsigned char *output;
  size_t output_capacity;
  const unsigned char *key;
  size_t key_size;
  const mulib_vmmdev_memory_t *memory;
} mulib_virtio_service_request_t;
typedef int (*mulib_virtio_service_fn)(void *context,
                                      const mulib_virtio_service_request_t *request,
                                      size_t *used);
/** @brief Return immutable metadata for an assigned ID, or NULL for holes/unknown IDs. */
const mulib_virtio_profile_t *mulib_virtio_profile(uint32_t id);
/** @brief Number of registry entries (48; IDs 0,14,15 are absent). */
size_t mulib_virtio_profile_count(void);
/** @brief Enumerate registry in ascending ID order; NULL past the end. */
const mulib_virtio_profile_t *mulib_virtio_profile_at(size_t index);
/** @brief Encode a standard 16-byte block request header. */
void mulib_virtio_blk_header(unsigned char out[16], uint32_t type, uint64_t sector);
/** @brief Encode an 8-byte input event. */
void mulib_virtio_input_event(unsigned char out[8], uint16_t type, uint16_t code, uint32_t value);
/** @brief Encode an 8-byte GPIO request, rejecting unknown operations/values. */
int mulib_virtio_gpio_request(unsigned char out[8], uint16_t type, uint16_t line, uint32_t value);
/** @brief Encode an I2C header; address is the wire-encoded 7/10-bit address. */
int mulib_virtio_i2c_header(unsigned char out[8], uint16_t address, uint32_t flags);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_VIRTIO) && !defined(MULIB_VIRTIO_IMPLEMENTED)
#define MULIB_VIRTIO_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
static const mulib_virtio_profile_t mulib_virtio_profiles_[] = {
  {1,"net",1,2,6,UINT64_C(1)<<5,0},
  {2,"blk",1,1,8,(UINT64_C(1)<<5)|(UINT64_C(1)<<9),0},
  {3,"console",1,2,0,0,0}, {4,"rng",1,1,0,0,0},
  {5,"balloon",1,2,8,UINT64_C(1)<<1,UINT64_C(1)<<1},
  {6,"ioMemory",3,0,0,0,0}, {7,"rpmsg",1,2,0,0,0},
  {8,"scsi",1,3,36,0,0}, {9,"9p",1,1,0,1,1},
  {10,"mac80211-wlan",3,0,0,0,0}, {11,"rproc-serial",3,0,0,0,0},
  {12,"caif",3,0,0,0,0}, {13,"memory-balloon",3,0,0,0,0},
  {16,"gpu",1,2,16,0,0}, {17,"rtc",3,0,0,0,0},
  {18,"input",1,2,136,0,0}, {19,"vsock",1,3,8,0,0},
  {20,"crypto",1,2,56,0,0}, {21,"signal-dist",3,0,0,0,0},
  {22,"pstore",3,0,0,0,0}, {23,"iommu",1,2,8,4,4},
  {24,"mem",1,1,56,0,0}, {25,"snd",1,4,12,0,0},
  {26,"fs",1,2,40,0,0}, {27,"pmem",1,1,16,0,0},
  {28,"rpmb",1,1,3,0,0}, {29,"hwsim",3,0,0,0,0},
  {30,"video-encoder",3,0,0,0,0}, {31,"video-decoder",3,0,0,0,0},
  {32,"scmi",1,1,0,0,0}, {33,"nitro-secure-module",3,0,0,0,0},
  {34,"i2c",1,1,0,1,1}, {35,"watchdog",3,0,0,0,0},
  {36,"can",3,0,0,0,0}, {37,"dma-buf",3,0,0,0,0},
  {38,"parameter-server",3,0,0,0,0}, {39,"audio-policy",3,0,0,0,0},
  {40,"bluetooth",3,0,0,0,0}, {41,"gpio",1,1,8,0,0},
  {42,"rdma",3,0,0,0,0}, {43,"camera",3,0,0,0,0},
  {44,"ism",3,0,0,0,0}, {45,"spi",3,0,0,0,0},
  {46,"tee",3,0,0,0,0}, {47,"cpu-balloon",3,0,0,0,0},
  {48,"media",3,0,0,0,0}, {49,"usb",3,0,0,0,0},
  {50,"video",3,0,0,0,0}
};
size_t mulib_virtio_profile_count(void)
{ return sizeof mulib_virtio_profiles_ / sizeof mulib_virtio_profiles_[0]; }
const mulib_virtio_profile_t *mulib_virtio_profile_at(size_t i)
{ return i < mulib_virtio_profile_count() ? mulib_virtio_profiles_ + i : NULL; }
const mulib_virtio_profile_t *mulib_virtio_profile(uint32_t id)
{
  for (size_t i = 0; i < mulib_virtio_profile_count(); ++i)
    if (mulib_virtio_profiles_[i].id == id) return mulib_virtio_profiles_ + i;
  return NULL;
}
void mulib_virtio_blk_header(unsigned char out[16], uint32_t type, uint64_t sector)
{
  mulib_endian_store_le32(out, type); mulib_endian_store_le32(out + 4, 0);
  mulib_vmmdev_store64(out + 8, sector);
}
void mulib_virtio_input_event(unsigned char out[8], uint16_t type, uint16_t code, uint32_t value)
{
  mulib_vmmdev_store16(out, type); mulib_vmmdev_store16(out + 2, code);
  mulib_endian_store_le32(out + 4, value);
}
int mulib_virtio_gpio_request(unsigned char out[8], uint16_t type, uint16_t line, uint32_t value)
{
  if (!out || type < 1 || type > 5 || (type == 3 && value > 2)
      || (type == 5 && value > 1) || ((type == 1 || type == 2 || type == 4) && value)
      || (type == 1 && line)) return MULIB_VMMDEV_EINVAL;
  mulib_vmmdev_store16(out, type); mulib_vmmdev_store16(out + 2, line);
  mulib_endian_store_le32(out + 4, value); return MULIB_VMMDEV_OK;
}
int mulib_virtio_i2c_header(unsigned char out[8], uint16_t address, uint32_t flags)
{
  if (!out || flags > 3 || (address & 1)
      || (address > 254 && (address & 0xf8) != 0xf0)) return MULIB_VMMDEV_EINVAL;
  mulib_vmmdev_store16(out, address); mulib_vmmdev_store16(out + 2, 0);
  mulib_endian_store_le32(out + 4, flags); return MULIB_VMMDEV_OK;
}
#ifdef __cplusplus
}
#endif
#endif
