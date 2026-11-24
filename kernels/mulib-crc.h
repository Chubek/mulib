/** @file mulib-crc.h
 * @brief Parameterized 1..128-bit CRCs and all 113 CRC RevEng presets.
 * Parameterized Rocksoft CRC engine for widths 1..128, with the complete
 * 113-entry CRC RevEng catalogue (retrieved 2026-11-10; canonical names).
 * Define MULIB_IMPL_CRC once. No allocation. Polynomial omits the leading bit.
 * Values use {lo,hi}; state is unreflected, final applies refout and xorout.
 * Update returns 1/0; NULL data is valid only for zero bytes. final is a snapshot.
 * Named wrappers return zero on invalid input (also a possible valid checksum).
 * CRCs detect accidental corruption; they provide no authentication.
 */
#ifndef MULIB_CRC_H
#define MULIB_CRC_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { uint64_t lo, hi; } mulib_crc_value;
typedef struct {
  const char *name;
  unsigned width;
  mulib_crc_value polynomial, initial, xorout, check;
  unsigned refin, refout;
} mulib_crc_params;
typedef struct { mulib_crc_params params; mulib_crc_value state; unsigned active; } mulib_crc_ctx;
typedef enum {
  MULIB_CRC_3_GSM,
  MULIB_CRC_3_ROHC,
  MULIB_CRC_4_G_704,
  MULIB_CRC_4_INTERLAKEN,
  MULIB_CRC_5_EPC_C1G2,
  MULIB_CRC_5_G_704,
  MULIB_CRC_5_USB,
  MULIB_CRC_6_CDMA2000_A,
  MULIB_CRC_6_CDMA2000_B,
  MULIB_CRC_6_DARC,
  MULIB_CRC_6_G_704,
  MULIB_CRC_6_GSM,
  MULIB_CRC_7_MMC,
  MULIB_CRC_7_ROHC,
  MULIB_CRC_7_UMTS,
  MULIB_CRC_8_AUTOSAR,
  MULIB_CRC_8_BLUETOOTH,
  MULIB_CRC_8_CDMA2000,
  MULIB_CRC_8_DARC,
  MULIB_CRC_8_DVB_S2,
  MULIB_CRC_8_GSM_A,
  MULIB_CRC_8_GSM_B,
  MULIB_CRC_8_HITAG,
  MULIB_CRC_8_I_432_1,
  MULIB_CRC_8_I_CODE,
  MULIB_CRC_8_LTE,
  MULIB_CRC_8_MAXIM_DOW,
  MULIB_CRC_8_MIFARE_MAD,
  MULIB_CRC_8_NRSC_5,
  MULIB_CRC_8_OPENSAFETY,
  MULIB_CRC_8_ROHC,
  MULIB_CRC_8_SAE_J1850,
  MULIB_CRC_8_SMBUS,
  MULIB_CRC_8_TECH_3250,
  MULIB_CRC_8_WCDMA,
  MULIB_CRC_10_ATM,
  MULIB_CRC_10_CDMA2000,
  MULIB_CRC_10_GSM,
  MULIB_CRC_11_FLEXRAY,
  MULIB_CRC_11_UMTS,
  MULIB_CRC_12_CDMA2000,
  MULIB_CRC_12_DECT,
  MULIB_CRC_12_GSM,
  MULIB_CRC_12_UMTS,
  MULIB_CRC_13_BBC,
  MULIB_CRC_14_DARC,
  MULIB_CRC_14_GSM,
  MULIB_CRC_15_CAN,
  MULIB_CRC_15_MPT1327,
  MULIB_CRC_16_ARC,
  MULIB_CRC_16_CDMA2000,
  MULIB_CRC_16_CMS,
  MULIB_CRC_16_DDS_110,
  MULIB_CRC_16_DECT_R,
  MULIB_CRC_16_DECT_X,
  MULIB_CRC_16_DNP,
  MULIB_CRC_16_EN_13757,
  MULIB_CRC_16_GENIBUS,
  MULIB_CRC_16_GSM,
  MULIB_CRC_16_IBM_3740,
  MULIB_CRC_16_IBM_SDLC,
  MULIB_CRC_16_ISO_IEC_14443_3_A,
  MULIB_CRC_16_KERMIT,
  MULIB_CRC_16_LJ1200,
  MULIB_CRC_16_M17,
  MULIB_CRC_16_MAXIM_DOW,
  MULIB_CRC_16_MCRF4XX,
  MULIB_CRC_16_MODBUS,
  MULIB_CRC_16_NRSC_5,
  MULIB_CRC_16_OPENSAFETY_A,
  MULIB_CRC_16_OPENSAFETY_B,
  MULIB_CRC_16_PROFIBUS,
  MULIB_CRC_16_RIELLO,
  MULIB_CRC_16_SPI_FUJITSU,
  MULIB_CRC_16_T10_DIF,
  MULIB_CRC_16_TELEDISK,
  MULIB_CRC_16_TMS37157,
  MULIB_CRC_16_UMTS,
  MULIB_CRC_16_USB,
  MULIB_CRC_16_XMODEM,
  MULIB_CRC_17_CAN_FD,
  MULIB_CRC_21_CAN_FD,
  MULIB_CRC_24_BLE,
  MULIB_CRC_24_FLEXRAY_A,
  MULIB_CRC_24_FLEXRAY_B,
  MULIB_CRC_24_INTERLAKEN,
  MULIB_CRC_24_LTE_A,
  MULIB_CRC_24_LTE_B,
  MULIB_CRC_24_OPENPGP,
  MULIB_CRC_24_OS_9,
  MULIB_CRC_30_CDMA,
  MULIB_CRC_31_PHILIPS,
  MULIB_CRC_32_AIXM,
  MULIB_CRC_32_AUTOSAR,
  MULIB_CRC_32_BASE91_D,
  MULIB_CRC_32_BZIP2,
  MULIB_CRC_32_CD_ROM_EDC,
  MULIB_CRC_32_CKSUM,
  MULIB_CRC_32_ISCSI,
  MULIB_CRC_32_ISO_HDLC,
  MULIB_CRC_32_JAMCRC,
  MULIB_CRC_32_MEF,
  MULIB_CRC_32_MPEG_2,
  MULIB_CRC_32_XFER,
  MULIB_CRC_40_GSM,
  MULIB_CRC_64_ECMA_182,
  MULIB_CRC_64_GO_ISO,
  MULIB_CRC_64_MS,
  MULIB_CRC_64_NVME,
  MULIB_CRC_64_REDIS,
  MULIB_CRC_64_WE,
  MULIB_CRC_64_XZ,
  MULIB_CRC_82_DARC,
  MULIB_CRC_PRESET_COUNT
} mulib_crc_preset;
extern const mulib_crc_params mulib_crc_catalogue[MULIB_CRC_PRESET_COUNT];
const mulib_crc_params *mulib_crc_find(const char *name);
int mulib_crc_init(mulib_crc_ctx *ctx, const mulib_crc_params *params);
int mulib_crc_update(mulib_crc_ctx *ctx, const void *data, size_t length);
mulib_crc_value mulib_crc_final(const mulib_crc_ctx *ctx);
int mulib_crc_compute(const mulib_crc_params *params, const void *data,
                      size_t length, mulib_crc_value *out);
uint32_t mulib_crc32c_intrinsics(uint32_t seed, const void *data, size_t length);
uint64_t mulib_crc_3_gsm(const void *data, size_t length);
uint64_t mulib_crc_3_rohc(const void *data, size_t length);
uint64_t mulib_crc_4_g_704(const void *data, size_t length);
uint64_t mulib_crc_4_interlaken(const void *data, size_t length);
uint64_t mulib_crc_5_epc_c1g2(const void *data, size_t length);
uint64_t mulib_crc_5_g_704(const void *data, size_t length);
uint64_t mulib_crc_5_usb(const void *data, size_t length);
uint64_t mulib_crc_6_cdma2000_a(const void *data, size_t length);
uint64_t mulib_crc_6_cdma2000_b(const void *data, size_t length);
uint64_t mulib_crc_6_darc(const void *data, size_t length);
uint64_t mulib_crc_6_g_704(const void *data, size_t length);
uint64_t mulib_crc_6_gsm(const void *data, size_t length);
uint64_t mulib_crc_7_mmc(const void *data, size_t length);
uint64_t mulib_crc_7_rohc(const void *data, size_t length);
uint64_t mulib_crc_7_umts(const void *data, size_t length);
uint64_t mulib_crc_8_autosar(const void *data, size_t length);
uint64_t mulib_crc_8_bluetooth(const void *data, size_t length);
uint64_t mulib_crc_8_cdma2000(const void *data, size_t length);
uint64_t mulib_crc_8_darc(const void *data, size_t length);
uint64_t mulib_crc_8_dvb_s2(const void *data, size_t length);
uint64_t mulib_crc_8_gsm_a(const void *data, size_t length);
uint64_t mulib_crc_8_gsm_b(const void *data, size_t length);
uint64_t mulib_crc_8_hitag(const void *data, size_t length);
uint64_t mulib_crc_8_i_432_1(const void *data, size_t length);
uint64_t mulib_crc_8_i_code(const void *data, size_t length);
uint64_t mulib_crc_8_lte(const void *data, size_t length);
uint64_t mulib_crc_8_maxim_dow(const void *data, size_t length);
uint64_t mulib_crc_8_mifare_mad(const void *data, size_t length);
uint64_t mulib_crc_8_nrsc_5(const void *data, size_t length);
uint64_t mulib_crc_8_opensafety(const void *data, size_t length);
uint64_t mulib_crc_8_rohc(const void *data, size_t length);
uint64_t mulib_crc_8_sae_j1850(const void *data, size_t length);
uint64_t mulib_crc_8_smbus(const void *data, size_t length);
uint64_t mulib_crc_8_tech_3250(const void *data, size_t length);
uint64_t mulib_crc_8_wcdma(const void *data, size_t length);
uint64_t mulib_crc_10_atm(const void *data, size_t length);
uint64_t mulib_crc_10_cdma2000(const void *data, size_t length);
uint64_t mulib_crc_10_gsm(const void *data, size_t length);
uint64_t mulib_crc_11_flexray(const void *data, size_t length);
uint64_t mulib_crc_11_umts(const void *data, size_t length);
uint64_t mulib_crc_12_cdma2000(const void *data, size_t length);
uint64_t mulib_crc_12_dect(const void *data, size_t length);
uint64_t mulib_crc_12_gsm(const void *data, size_t length);
uint64_t mulib_crc_12_umts(const void *data, size_t length);
uint64_t mulib_crc_13_bbc(const void *data, size_t length);
uint64_t mulib_crc_14_darc(const void *data, size_t length);
uint64_t mulib_crc_14_gsm(const void *data, size_t length);
uint64_t mulib_crc_15_can(const void *data, size_t length);
uint64_t mulib_crc_15_mpt1327(const void *data, size_t length);
uint64_t mulib_crc_16_arc(const void *data, size_t length);
uint64_t mulib_crc_16_cdma2000(const void *data, size_t length);
uint64_t mulib_crc_16_cms(const void *data, size_t length);
uint64_t mulib_crc_16_dds_110(const void *data, size_t length);
uint64_t mulib_crc_16_dect_r(const void *data, size_t length);
uint64_t mulib_crc_16_dect_x(const void *data, size_t length);
uint64_t mulib_crc_16_dnp(const void *data, size_t length);
uint64_t mulib_crc_16_en_13757(const void *data, size_t length);
uint64_t mulib_crc_16_genibus(const void *data, size_t length);
uint64_t mulib_crc_16_gsm(const void *data, size_t length);
uint64_t mulib_crc_16_ibm_3740(const void *data, size_t length);
uint64_t mulib_crc_16_ibm_sdlc(const void *data, size_t length);
uint64_t mulib_crc_16_iso_iec_14443_3_a(const void *data, size_t length);
uint64_t mulib_crc_16_kermit(const void *data, size_t length);
uint64_t mulib_crc_16_lj1200(const void *data, size_t length);
uint64_t mulib_crc_16_m17(const void *data, size_t length);
uint64_t mulib_crc_16_maxim_dow(const void *data, size_t length);
uint64_t mulib_crc_16_mcrf4xx(const void *data, size_t length);
uint64_t mulib_crc_16_modbus(const void *data, size_t length);
uint64_t mulib_crc_16_nrsc_5(const void *data, size_t length);
uint64_t mulib_crc_16_opensafety_a(const void *data, size_t length);
uint64_t mulib_crc_16_opensafety_b(const void *data, size_t length);
uint64_t mulib_crc_16_profibus(const void *data, size_t length);
uint64_t mulib_crc_16_riello(const void *data, size_t length);
uint64_t mulib_crc_16_spi_fujitsu(const void *data, size_t length);
uint64_t mulib_crc_16_t10_dif(const void *data, size_t length);
uint64_t mulib_crc_16_teledisk(const void *data, size_t length);
uint64_t mulib_crc_16_tms37157(const void *data, size_t length);
uint64_t mulib_crc_16_umts(const void *data, size_t length);
uint64_t mulib_crc_16_usb(const void *data, size_t length);
uint64_t mulib_crc_16_xmodem(const void *data, size_t length);
uint64_t mulib_crc_17_can_fd(const void *data, size_t length);
uint64_t mulib_crc_21_can_fd(const void *data, size_t length);
uint64_t mulib_crc_24_ble(const void *data, size_t length);
uint64_t mulib_crc_24_flexray_a(const void *data, size_t length);
uint64_t mulib_crc_24_flexray_b(const void *data, size_t length);
uint64_t mulib_crc_24_interlaken(const void *data, size_t length);
uint64_t mulib_crc_24_lte_a(const void *data, size_t length);
uint64_t mulib_crc_24_lte_b(const void *data, size_t length);
uint64_t mulib_crc_24_openpgp(const void *data, size_t length);
uint64_t mulib_crc_24_os_9(const void *data, size_t length);
uint64_t mulib_crc_30_cdma(const void *data, size_t length);
uint64_t mulib_crc_31_philips(const void *data, size_t length);
uint64_t mulib_crc_32_aixm(const void *data, size_t length);
uint64_t mulib_crc_32_autosar(const void *data, size_t length);
uint64_t mulib_crc_32_base91_d(const void *data, size_t length);
uint64_t mulib_crc_32_bzip2(const void *data, size_t length);
uint64_t mulib_crc_32_cd_rom_edc(const void *data, size_t length);
uint64_t mulib_crc_32_cksum(const void *data, size_t length);
uint64_t mulib_crc_32_iscsi(const void *data, size_t length);
uint64_t mulib_crc_32_iso_hdlc(const void *data, size_t length);
uint64_t mulib_crc_32_jamcrc(const void *data, size_t length);
uint64_t mulib_crc_32_mef(const void *data, size_t length);
uint64_t mulib_crc_32_mpeg_2(const void *data, size_t length);
uint64_t mulib_crc_32_xfer(const void *data, size_t length);
uint64_t mulib_crc_40_gsm(const void *data, size_t length);
uint64_t mulib_crc_64_ecma_182(const void *data, size_t length);
uint64_t mulib_crc_64_go_iso(const void *data, size_t length);
uint64_t mulib_crc_64_ms(const void *data, size_t length);
uint64_t mulib_crc_64_nvme(const void *data, size_t length);
uint64_t mulib_crc_64_redis(const void *data, size_t length);
uint64_t mulib_crc_64_we(const void *data, size_t length);
uint64_t mulib_crc_64_xz(const void *data, size_t length);
mulib_crc_value mulib_crc_82_darc(const void *data, size_t length);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CRC) && !defined(MULIB_CRC_IMPLEMENTED)
#define MULIB_CRC_IMPLEMENTED
#include <string.h>
#if defined(__SSE4_2__) && !defined(MULIB_HASH_NO_INTRINSICS)
#include <nmmintrin.h>
#endif
#ifdef __cplusplus
extern "C" {
#endif
const mulib_crc_params mulib_crc_catalogue[MULIB_CRC_PRESET_COUNT]={
  {"CRC-3/GSM",3,{UINT64_C(0x3),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x7),UINT64_C(0x0)},{UINT64_C(0x4),UINT64_C(0x0)},0,0},
  {"CRC-3/ROHC",3,{UINT64_C(0x3),UINT64_C(0x0)},{UINT64_C(0x7),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x6),UINT64_C(0x0)},1,1},
  {"CRC-4/G-704",4,{UINT64_C(0x3),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x7),UINT64_C(0x0)},1,1},
  {"CRC-4/INTERLAKEN",4,{UINT64_C(0x3),UINT64_C(0x0)},{UINT64_C(0xf),UINT64_C(0x0)},{UINT64_C(0xf),UINT64_C(0x0)},{UINT64_C(0xb),UINT64_C(0x0)},0,0},
  {"CRC-5/EPC-C1G2",5,{UINT64_C(0x9),UINT64_C(0x0)},{UINT64_C(0x9),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},0,0},
  {"CRC-5/G-704",5,{UINT64_C(0x15),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x7),UINT64_C(0x0)},1,1},
  {"CRC-5/USB",5,{UINT64_C(0x5),UINT64_C(0x0)},{UINT64_C(0x1f),UINT64_C(0x0)},{UINT64_C(0x1f),UINT64_C(0x0)},{UINT64_C(0x19),UINT64_C(0x0)},1,1},
  {"CRC-6/CDMA2000-A",6,{UINT64_C(0x27),UINT64_C(0x0)},{UINT64_C(0x3f),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0xd),UINT64_C(0x0)},0,0},
  {"CRC-6/CDMA2000-B",6,{UINT64_C(0x7),UINT64_C(0x0)},{UINT64_C(0x3f),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x3b),UINT64_C(0x0)},0,0},
  {"CRC-6/DARC",6,{UINT64_C(0x19),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x26),UINT64_C(0x0)},1,1},
  {"CRC-6/G-704",6,{UINT64_C(0x3),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x6),UINT64_C(0x0)},1,1},
  {"CRC-6/GSM",6,{UINT64_C(0x2f),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x3f),UINT64_C(0x0)},{UINT64_C(0x13),UINT64_C(0x0)},0,0},
  {"CRC-7/MMC",7,{UINT64_C(0x9),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x75),UINT64_C(0x0)},0,0},
  {"CRC-7/ROHC",7,{UINT64_C(0x4f),UINT64_C(0x0)},{UINT64_C(0x7f),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x53),UINT64_C(0x0)},1,1},
  {"CRC-7/UMTS",7,{UINT64_C(0x45),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x61),UINT64_C(0x0)},0,0},
  {"CRC-8/AUTOSAR",8,{UINT64_C(0x2f),UINT64_C(0x0)},{UINT64_C(0xff),UINT64_C(0x0)},{UINT64_C(0xff),UINT64_C(0x0)},{UINT64_C(0xdf),UINT64_C(0x0)},0,0},
  {"CRC-8/BLUETOOTH",8,{UINT64_C(0xa7),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x26),UINT64_C(0x0)},1,1},
  {"CRC-8/CDMA2000",8,{UINT64_C(0x9b),UINT64_C(0x0)},{UINT64_C(0xff),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0xda),UINT64_C(0x0)},0,0},
  {"CRC-8/DARC",8,{UINT64_C(0x39),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x15),UINT64_C(0x0)},1,1},
  {"CRC-8/DVB-S2",8,{UINT64_C(0xd5),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0xbc),UINT64_C(0x0)},0,0},
  {"CRC-8/GSM-A",8,{UINT64_C(0x1d),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x37),UINT64_C(0x0)},0,0},
  {"CRC-8/GSM-B",8,{UINT64_C(0x49),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0xff),UINT64_C(0x0)},{UINT64_C(0x94),UINT64_C(0x0)},0,0},
  {"CRC-8/HITAG",8,{UINT64_C(0x1d),UINT64_C(0x0)},{UINT64_C(0xff),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0xb4),UINT64_C(0x0)},0,0},
  {"CRC-8/I-432-1",8,{UINT64_C(0x7),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x55),UINT64_C(0x0)},{UINT64_C(0xa1),UINT64_C(0x0)},0,0},
  {"CRC-8/I-CODE",8,{UINT64_C(0x1d),UINT64_C(0x0)},{UINT64_C(0xfd),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x7e),UINT64_C(0x0)},0,0},
  {"CRC-8/LTE",8,{UINT64_C(0x9b),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0xea),UINT64_C(0x0)},0,0},
  {"CRC-8/MAXIM-DOW",8,{UINT64_C(0x31),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0xa1),UINT64_C(0x0)},1,1},
  {"CRC-8/MIFARE-MAD",8,{UINT64_C(0x1d),UINT64_C(0x0)},{UINT64_C(0xc7),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x99),UINT64_C(0x0)},0,0},
  {"CRC-8/NRSC-5",8,{UINT64_C(0x31),UINT64_C(0x0)},{UINT64_C(0xff),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0xf7),UINT64_C(0x0)},0,0},
  {"CRC-8/OPENSAFETY",8,{UINT64_C(0x2f),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x3e),UINT64_C(0x0)},0,0},
  {"CRC-8/ROHC",8,{UINT64_C(0x7),UINT64_C(0x0)},{UINT64_C(0xff),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0xd0),UINT64_C(0x0)},1,1},
  {"CRC-8/SAE-J1850",8,{UINT64_C(0x1d),UINT64_C(0x0)},{UINT64_C(0xff),UINT64_C(0x0)},{UINT64_C(0xff),UINT64_C(0x0)},{UINT64_C(0x4b),UINT64_C(0x0)},0,0},
  {"CRC-8/SMBUS",8,{UINT64_C(0x7),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0xf4),UINT64_C(0x0)},0,0},
  {"CRC-8/TECH-3250",8,{UINT64_C(0x1d),UINT64_C(0x0)},{UINT64_C(0xff),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x97),UINT64_C(0x0)},1,1},
  {"CRC-8/WCDMA",8,{UINT64_C(0x9b),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x25),UINT64_C(0x0)},1,1},
  {"CRC-10/ATM",10,{UINT64_C(0x233),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x199),UINT64_C(0x0)},0,0},
  {"CRC-10/CDMA2000",10,{UINT64_C(0x3d9),UINT64_C(0x0)},{UINT64_C(0x3ff),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x233),UINT64_C(0x0)},0,0},
  {"CRC-10/GSM",10,{UINT64_C(0x175),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x3ff),UINT64_C(0x0)},{UINT64_C(0x12a),UINT64_C(0x0)},0,0},
  {"CRC-11/FLEXRAY",11,{UINT64_C(0x385),UINT64_C(0x0)},{UINT64_C(0x1a),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x5a3),UINT64_C(0x0)},0,0},
  {"CRC-11/UMTS",11,{UINT64_C(0x307),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x61),UINT64_C(0x0)},0,0},
  {"CRC-12/CDMA2000",12,{UINT64_C(0xf13),UINT64_C(0x0)},{UINT64_C(0xfff),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0xd4d),UINT64_C(0x0)},0,0},
  {"CRC-12/DECT",12,{UINT64_C(0x80f),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0xf5b),UINT64_C(0x0)},0,0},
  {"CRC-12/GSM",12,{UINT64_C(0xd31),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0xfff),UINT64_C(0x0)},{UINT64_C(0xb34),UINT64_C(0x0)},0,0},
  {"CRC-12/UMTS",12,{UINT64_C(0x80f),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0xdaf),UINT64_C(0x0)},0,1},
  {"CRC-13/BBC",13,{UINT64_C(0x1cf5),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x4fa),UINT64_C(0x0)},0,0},
  {"CRC-14/DARC",14,{UINT64_C(0x805),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x82d),UINT64_C(0x0)},1,1},
  {"CRC-14/GSM",14,{UINT64_C(0x202d),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x3fff),UINT64_C(0x0)},{UINT64_C(0x30ae),UINT64_C(0x0)},0,0},
  {"CRC-15/CAN",15,{UINT64_C(0x4599),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x59e),UINT64_C(0x0)},0,0},
  {"CRC-15/MPT1327",15,{UINT64_C(0x6815),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x1),UINT64_C(0x0)},{UINT64_C(0x2566),UINT64_C(0x0)},0,0},
  {"CRC-16/ARC",16,{UINT64_C(0x8005),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0xbb3d),UINT64_C(0x0)},1,1},
  {"CRC-16/CDMA2000",16,{UINT64_C(0xc867),UINT64_C(0x0)},{UINT64_C(0xffff),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x4c06),UINT64_C(0x0)},0,0},
  {"CRC-16/CMS",16,{UINT64_C(0x8005),UINT64_C(0x0)},{UINT64_C(0xffff),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0xaee7),UINT64_C(0x0)},0,0},
  {"CRC-16/DDS-110",16,{UINT64_C(0x8005),UINT64_C(0x0)},{UINT64_C(0x800d),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x9ecf),UINT64_C(0x0)},0,0},
  {"CRC-16/DECT-R",16,{UINT64_C(0x589),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x1),UINT64_C(0x0)},{UINT64_C(0x7e),UINT64_C(0x0)},0,0},
  {"CRC-16/DECT-X",16,{UINT64_C(0x589),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x7f),UINT64_C(0x0)},0,0},
  {"CRC-16/DNP",16,{UINT64_C(0x3d65),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0xffff),UINT64_C(0x0)},{UINT64_C(0xea82),UINT64_C(0x0)},1,1},
  {"CRC-16/EN-13757",16,{UINT64_C(0x3d65),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0xffff),UINT64_C(0x0)},{UINT64_C(0xc2b7),UINT64_C(0x0)},0,0},
  {"CRC-16/GENIBUS",16,{UINT64_C(0x1021),UINT64_C(0x0)},{UINT64_C(0xffff),UINT64_C(0x0)},{UINT64_C(0xffff),UINT64_C(0x0)},{UINT64_C(0xd64e),UINT64_C(0x0)},0,0},
  {"CRC-16/GSM",16,{UINT64_C(0x1021),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0xffff),UINT64_C(0x0)},{UINT64_C(0xce3c),UINT64_C(0x0)},0,0},
  {"CRC-16/IBM-3740",16,{UINT64_C(0x1021),UINT64_C(0x0)},{UINT64_C(0xffff),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x29b1),UINT64_C(0x0)},0,0},
  {"CRC-16/IBM-SDLC",16,{UINT64_C(0x1021),UINT64_C(0x0)},{UINT64_C(0xffff),UINT64_C(0x0)},{UINT64_C(0xffff),UINT64_C(0x0)},{UINT64_C(0x906e),UINT64_C(0x0)},1,1},
  {"CRC-16/ISO-IEC-14443-3-A",16,{UINT64_C(0x1021),UINT64_C(0x0)},{UINT64_C(0xc6c6),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0xbf05),UINT64_C(0x0)},1,1},
  {"CRC-16/KERMIT",16,{UINT64_C(0x1021),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x2189),UINT64_C(0x0)},1,1},
  {"CRC-16/LJ1200",16,{UINT64_C(0x6f63),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0xbdf4),UINT64_C(0x0)},0,0},
  {"CRC-16/M17",16,{UINT64_C(0x5935),UINT64_C(0x0)},{UINT64_C(0xffff),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x772b),UINT64_C(0x0)},0,0},
  {"CRC-16/MAXIM-DOW",16,{UINT64_C(0x8005),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0xffff),UINT64_C(0x0)},{UINT64_C(0x44c2),UINT64_C(0x0)},1,1},
  {"CRC-16/MCRF4XX",16,{UINT64_C(0x1021),UINT64_C(0x0)},{UINT64_C(0xffff),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x6f91),UINT64_C(0x0)},1,1},
  {"CRC-16/MODBUS",16,{UINT64_C(0x8005),UINT64_C(0x0)},{UINT64_C(0xffff),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x4b37),UINT64_C(0x0)},1,1},
  {"CRC-16/NRSC-5",16,{UINT64_C(0x80b),UINT64_C(0x0)},{UINT64_C(0xffff),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0xa066),UINT64_C(0x0)},1,1},
  {"CRC-16/OPENSAFETY-A",16,{UINT64_C(0x5935),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x5d38),UINT64_C(0x0)},0,0},
  {"CRC-16/OPENSAFETY-B",16,{UINT64_C(0x755b),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x20fe),UINT64_C(0x0)},0,0},
  {"CRC-16/PROFIBUS",16,{UINT64_C(0x1dcf),UINT64_C(0x0)},{UINT64_C(0xffff),UINT64_C(0x0)},{UINT64_C(0xffff),UINT64_C(0x0)},{UINT64_C(0xa819),UINT64_C(0x0)},0,0},
  {"CRC-16/RIELLO",16,{UINT64_C(0x1021),UINT64_C(0x0)},{UINT64_C(0xb2aa),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x63d0),UINT64_C(0x0)},1,1},
  {"CRC-16/SPI-FUJITSU",16,{UINT64_C(0x1021),UINT64_C(0x0)},{UINT64_C(0x1d0f),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0xe5cc),UINT64_C(0x0)},0,0},
  {"CRC-16/T10-DIF",16,{UINT64_C(0x8bb7),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0xd0db),UINT64_C(0x0)},0,0},
  {"CRC-16/TELEDISK",16,{UINT64_C(0xa097),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0xfb3),UINT64_C(0x0)},0,0},
  {"CRC-16/TMS37157",16,{UINT64_C(0x1021),UINT64_C(0x0)},{UINT64_C(0x89ec),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x26b1),UINT64_C(0x0)},1,1},
  {"CRC-16/UMTS",16,{UINT64_C(0x8005),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0xfee8),UINT64_C(0x0)},0,0},
  {"CRC-16/USB",16,{UINT64_C(0x8005),UINT64_C(0x0)},{UINT64_C(0xffff),UINT64_C(0x0)},{UINT64_C(0xffff),UINT64_C(0x0)},{UINT64_C(0xb4c8),UINT64_C(0x0)},1,1},
  {"CRC-16/XMODEM",16,{UINT64_C(0x1021),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x31c3),UINT64_C(0x0)},0,0},
  {"CRC-17/CAN-FD",17,{UINT64_C(0x1685b),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x4f03),UINT64_C(0x0)},0,0},
  {"CRC-21/CAN-FD",21,{UINT64_C(0x102899),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0xed841),UINT64_C(0x0)},0,0},
  {"CRC-24/BLE",24,{UINT64_C(0x65b),UINT64_C(0x0)},{UINT64_C(0x555555),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0xc25a56),UINT64_C(0x0)},1,1},
  {"CRC-24/FLEXRAY-A",24,{UINT64_C(0x5d6dcb),UINT64_C(0x0)},{UINT64_C(0xfedcba),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x7979bd),UINT64_C(0x0)},0,0},
  {"CRC-24/FLEXRAY-B",24,{UINT64_C(0x5d6dcb),UINT64_C(0x0)},{UINT64_C(0xabcdef),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x1f23b8),UINT64_C(0x0)},0,0},
  {"CRC-24/INTERLAKEN",24,{UINT64_C(0x328b63),UINT64_C(0x0)},{UINT64_C(0xffffff),UINT64_C(0x0)},{UINT64_C(0xffffff),UINT64_C(0x0)},{UINT64_C(0xb4f3e6),UINT64_C(0x0)},0,0},
  {"CRC-24/LTE-A",24,{UINT64_C(0x864cfb),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0xcde703),UINT64_C(0x0)},0,0},
  {"CRC-24/LTE-B",24,{UINT64_C(0x800063),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x23ef52),UINT64_C(0x0)},0,0},
  {"CRC-24/OPENPGP",24,{UINT64_C(0x864cfb),UINT64_C(0x0)},{UINT64_C(0xb704ce),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x21cf02),UINT64_C(0x0)},0,0},
  {"CRC-24/OS-9",24,{UINT64_C(0x800063),UINT64_C(0x0)},{UINT64_C(0xffffff),UINT64_C(0x0)},{UINT64_C(0xffffff),UINT64_C(0x0)},{UINT64_C(0x200fa5),UINT64_C(0x0)},0,0},
  {"CRC-30/CDMA",30,{UINT64_C(0x2030b9c7),UINT64_C(0x0)},{UINT64_C(0x3fffffff),UINT64_C(0x0)},{UINT64_C(0x3fffffff),UINT64_C(0x0)},{UINT64_C(0x4c34abf),UINT64_C(0x0)},0,0},
  {"CRC-31/PHILIPS",31,{UINT64_C(0x4c11db7),UINT64_C(0x0)},{UINT64_C(0x7fffffff),UINT64_C(0x0)},{UINT64_C(0x7fffffff),UINT64_C(0x0)},{UINT64_C(0xce9e46c),UINT64_C(0x0)},0,0},
  {"CRC-32/AIXM",32,{UINT64_C(0x814141ab),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x3010bf7f),UINT64_C(0x0)},0,0},
  {"CRC-32/AUTOSAR",32,{UINT64_C(0xf4acfb13),UINT64_C(0x0)},{UINT64_C(0xffffffff),UINT64_C(0x0)},{UINT64_C(0xffffffff),UINT64_C(0x0)},{UINT64_C(0x1697d06a),UINT64_C(0x0)},1,1},
  {"CRC-32/BASE91-D",32,{UINT64_C(0xa833982b),UINT64_C(0x0)},{UINT64_C(0xffffffff),UINT64_C(0x0)},{UINT64_C(0xffffffff),UINT64_C(0x0)},{UINT64_C(0x87315576),UINT64_C(0x0)},1,1},
  {"CRC-32/BZIP2",32,{UINT64_C(0x4c11db7),UINT64_C(0x0)},{UINT64_C(0xffffffff),UINT64_C(0x0)},{UINT64_C(0xffffffff),UINT64_C(0x0)},{UINT64_C(0xfc891918),UINT64_C(0x0)},0,0},
  {"CRC-32/CD-ROM-EDC",32,{UINT64_C(0x8001801b),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x6ec2edc4),UINT64_C(0x0)},1,1},
  {"CRC-32/CKSUM",32,{UINT64_C(0x4c11db7),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0xffffffff),UINT64_C(0x0)},{UINT64_C(0x765e7680),UINT64_C(0x0)},0,0},
  {"CRC-32/ISCSI",32,{UINT64_C(0x1edc6f41),UINT64_C(0x0)},{UINT64_C(0xffffffff),UINT64_C(0x0)},{UINT64_C(0xffffffff),UINT64_C(0x0)},{UINT64_C(0xe3069283),UINT64_C(0x0)},1,1},
  {"CRC-32/ISO-HDLC",32,{UINT64_C(0x4c11db7),UINT64_C(0x0)},{UINT64_C(0xffffffff),UINT64_C(0x0)},{UINT64_C(0xffffffff),UINT64_C(0x0)},{UINT64_C(0xcbf43926),UINT64_C(0x0)},1,1},
  {"CRC-32/JAMCRC",32,{UINT64_C(0x4c11db7),UINT64_C(0x0)},{UINT64_C(0xffffffff),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x340bc6d9),UINT64_C(0x0)},1,1},
  {"CRC-32/MEF",32,{UINT64_C(0x741b8cd7),UINT64_C(0x0)},{UINT64_C(0xffffffff),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0xd2c22f51),UINT64_C(0x0)},1,1},
  {"CRC-32/MPEG-2",32,{UINT64_C(0x4c11db7),UINT64_C(0x0)},{UINT64_C(0xffffffff),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x376e6e7),UINT64_C(0x0)},0,0},
  {"CRC-32/XFER",32,{UINT64_C(0xaf),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0xbd0be338),UINT64_C(0x0)},0,0},
  {"CRC-40/GSM",40,{UINT64_C(0x4820009),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0xffffffffff),UINT64_C(0x0)},{UINT64_C(0xd4164fc646),UINT64_C(0x0)},0,0},
  {"CRC-64/ECMA-182",64,{UINT64_C(0x42f0e1eba9ea3693),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x6c40df5f0b497347),UINT64_C(0x0)},0,0},
  {"CRC-64/GO-ISO",64,{UINT64_C(0x1b),UINT64_C(0x0)},{UINT64_C(0xffffffffffffffff),UINT64_C(0x0)},{UINT64_C(0xffffffffffffffff),UINT64_C(0x0)},{UINT64_C(0xb90956c775a41001),UINT64_C(0x0)},1,1},
  {"CRC-64/MS",64,{UINT64_C(0x259c84cba6426349),UINT64_C(0x0)},{UINT64_C(0xffffffffffffffff),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x75d4b74f024eceea),UINT64_C(0x0)},1,1},
  {"CRC-64/NVME",64,{UINT64_C(0xad93d23594c93659),UINT64_C(0x0)},{UINT64_C(0xffffffffffffffff),UINT64_C(0x0)},{UINT64_C(0xffffffffffffffff),UINT64_C(0x0)},{UINT64_C(0xae8b14860a799888),UINT64_C(0x0)},1,1},
  {"CRC-64/REDIS",64,{UINT64_C(0xad93d23594c935a9),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0xe9c6d914c4b8d9ca),UINT64_C(0x0)},1,1},
  {"CRC-64/WE",64,{UINT64_C(0x42f0e1eba9ea3693),UINT64_C(0x0)},{UINT64_C(0xffffffffffffffff),UINT64_C(0x0)},{UINT64_C(0xffffffffffffffff),UINT64_C(0x0)},{UINT64_C(0x62ec59e3f1a4f00a),UINT64_C(0x0)},0,0},
  {"CRC-64/XZ",64,{UINT64_C(0x42f0e1eba9ea3693),UINT64_C(0x0)},{UINT64_C(0xffffffffffffffff),UINT64_C(0x0)},{UINT64_C(0xffffffffffffffff),UINT64_C(0x0)},{UINT64_C(0x995dc9bbdf1939fa),UINT64_C(0x0)},1,1},
  {"CRC-82/DARC",82,{UINT64_C(0x111011401440411),UINT64_C(0x308c)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x0),UINT64_C(0x0)},{UINT64_C(0x3f625023801fd612),UINT64_C(0x9ea8)},1,1},
};
static mulib_crc_value mulib_crc_shl_(mulib_crc_value x)
{ mulib_crc_value r={x.lo<<1,(x.hi<<1)|(x.lo>>63)}; return r; }
static unsigned mulib_crc_bit_(mulib_crc_value x,unsigned bit)
{ return (unsigned)((bit<64 ? x.lo>>bit : x.hi>>(bit-64))&1); }
static mulib_crc_value mulib_crc_mask_(mulib_crc_value x,unsigned width)
{
  if(width<64) { x.lo&=(UINT64_C(1)<<width)-1; x.hi=0; }
  else if(width==64) x.hi=0;
  else if(width<128) x.hi&=(UINT64_C(1)<<(width-64))-1;
  return x;
}
static int mulib_crc_fits_(mulib_crc_value x,unsigned w)
{ mulib_crc_value m=mulib_crc_mask_(x,w); return x.lo==m.lo && x.hi==m.hi; }
const mulib_crc_params *mulib_crc_find(const char *name)
{
  if(name) for(unsigned i=0;i<MULIB_CRC_PRESET_COUNT;++i)
    if(!strcmp(name,mulib_crc_catalogue[i].name)) return mulib_crc_catalogue+i;
  return NULL;
}
int mulib_crc_init(mulib_crc_ctx *c,const mulib_crc_params *p)
{
  if(!c || !p || !p->width || p->width>128 || p->refin>1 || p->refout>1
     || !mulib_crc_fits_(p->polynomial,p->width)
     || !mulib_crc_fits_(p->initial,p->width) || !mulib_crc_fits_(p->xorout,p->width)) return 0;
  c->params=*p; c->state=p->initial; c->active=1; return 1;
}
int mulib_crc_update(mulib_crc_ctx *c,const void *data,size_t n)
{
  const unsigned char *p=(const unsigned char *)data;
  if(!c || c->active!=1 || (!p && n)) return 0;
  while(n--) {
    unsigned byte=*p++;
    for(unsigned j=0;j<8;++j) {
      unsigned bit=mulib_crc_bit_(c->state,c->params.width-1)
        ^ ((byte>>(c->params.refin ? j : 7-j))&1);
      c->state=mulib_crc_shl_(c->state);
      if(bit) { c->state.lo^=c->params.polynomial.lo; c->state.hi^=c->params.polynomial.hi; }
    }
    c->state=mulib_crc_mask_(c->state,c->params.width);
  }
  return 1;
}
mulib_crc_value mulib_crc_final(const mulib_crc_ctx *c)
{
  mulib_crc_value r={0,0};
  if(!c || c->active!=1) return r;
  if(c->params.refout) for(unsigned i=0;i<c->params.width;++i) {
    r=mulib_crc_shl_(r); r.lo|=mulib_crc_bit_(c->state,i);
  } else r=c->state;
  r.lo^=c->params.xorout.lo; r.hi^=c->params.xorout.hi; return r;
}
int mulib_crc_compute(const mulib_crc_params *p,const void *data,size_t n,mulib_crc_value *out)
{
  mulib_crc_ctx c;
  if(!out || !mulib_crc_init(&c,p) || !mulib_crc_update(&c,data,n)) return 0;
  *out=mulib_crc_final(&c); return 1;
}
uint32_t mulib_crc32c_intrinsics(uint32_t seed,const void *data,size_t n)
{
  const unsigned char *p=(const unsigned char *)data;
  uint32_t c=~seed;
  if(!p && n) return seed;
#if defined(__SSE4_2__) && !defined(MULIB_HASH_NO_INTRINSICS)
#if defined(__x86_64__) || defined(_M_X64)
  while(n>=8) {
    uint64_t word; memcpy(&word,p,8); c=(uint32_t)_mm_crc32_u64(c,word); p+=8; n-=8;
  }
#endif
  while(n>=4) {
    uint32_t word; memcpy(&word,p,4); c=_mm_crc32_u32(c,word); p+=4; n-=4;
  }
  while(n--) c=_mm_crc32_u8(c,*p++);
#else
  while(n--) {
    c^=*p++;
    for(unsigned j=0;j<8;++j) c=(c>>1)^((0u-(c&1))&UINT32_C(0x82f63b78));
  }
#endif
  return ~c;
}
uint64_t mulib_crc_3_gsm(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_3_GSM],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_3_rohc(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_3_ROHC],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_4_g_704(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_4_G_704],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_4_interlaken(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_4_INTERLAKEN],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_5_epc_c1g2(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_5_EPC_C1G2],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_5_g_704(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_5_G_704],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_5_usb(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_5_USB],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_6_cdma2000_a(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_6_CDMA2000_A],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_6_cdma2000_b(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_6_CDMA2000_B],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_6_darc(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_6_DARC],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_6_g_704(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_6_G_704],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_6_gsm(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_6_GSM],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_7_mmc(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_7_MMC],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_7_rohc(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_7_ROHC],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_7_umts(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_7_UMTS],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_8_autosar(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_8_AUTOSAR],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_8_bluetooth(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_8_BLUETOOTH],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_8_cdma2000(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_8_CDMA2000],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_8_darc(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_8_DARC],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_8_dvb_s2(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_8_DVB_S2],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_8_gsm_a(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_8_GSM_A],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_8_gsm_b(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_8_GSM_B],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_8_hitag(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_8_HITAG],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_8_i_432_1(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_8_I_432_1],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_8_i_code(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_8_I_CODE],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_8_lte(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_8_LTE],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_8_maxim_dow(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_8_MAXIM_DOW],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_8_mifare_mad(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_8_MIFARE_MAD],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_8_nrsc_5(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_8_NRSC_5],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_8_opensafety(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_8_OPENSAFETY],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_8_rohc(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_8_ROHC],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_8_sae_j1850(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_8_SAE_J1850],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_8_smbus(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_8_SMBUS],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_8_tech_3250(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_8_TECH_3250],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_8_wcdma(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_8_WCDMA],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_10_atm(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_10_ATM],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_10_cdma2000(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_10_CDMA2000],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_10_gsm(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_10_GSM],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_11_flexray(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_11_FLEXRAY],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_11_umts(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_11_UMTS],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_12_cdma2000(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_12_CDMA2000],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_12_dect(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_12_DECT],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_12_gsm(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_12_GSM],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_12_umts(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_12_UMTS],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_13_bbc(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_13_BBC],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_14_darc(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_14_DARC],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_14_gsm(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_14_GSM],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_15_can(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_15_CAN],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_15_mpt1327(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_15_MPT1327],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_16_arc(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_16_ARC],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_16_cdma2000(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_16_CDMA2000],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_16_cms(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_16_CMS],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_16_dds_110(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_16_DDS_110],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_16_dect_r(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_16_DECT_R],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_16_dect_x(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_16_DECT_X],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_16_dnp(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_16_DNP],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_16_en_13757(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_16_EN_13757],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_16_genibus(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_16_GENIBUS],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_16_gsm(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_16_GSM],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_16_ibm_3740(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_16_IBM_3740],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_16_ibm_sdlc(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_16_IBM_SDLC],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_16_iso_iec_14443_3_a(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_16_ISO_IEC_14443_3_A],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_16_kermit(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_16_KERMIT],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_16_lj1200(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_16_LJ1200],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_16_m17(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_16_M17],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_16_maxim_dow(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_16_MAXIM_DOW],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_16_mcrf4xx(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_16_MCRF4XX],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_16_modbus(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_16_MODBUS],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_16_nrsc_5(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_16_NRSC_5],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_16_opensafety_a(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_16_OPENSAFETY_A],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_16_opensafety_b(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_16_OPENSAFETY_B],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_16_profibus(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_16_PROFIBUS],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_16_riello(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_16_RIELLO],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_16_spi_fujitsu(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_16_SPI_FUJITSU],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_16_t10_dif(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_16_T10_DIF],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_16_teledisk(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_16_TELEDISK],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_16_tms37157(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_16_TMS37157],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_16_umts(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_16_UMTS],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_16_usb(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_16_USB],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_16_xmodem(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_16_XMODEM],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_17_can_fd(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_17_CAN_FD],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_21_can_fd(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_21_CAN_FD],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_24_ble(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_24_BLE],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_24_flexray_a(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_24_FLEXRAY_A],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_24_flexray_b(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_24_FLEXRAY_B],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_24_interlaken(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_24_INTERLAKEN],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_24_lte_a(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_24_LTE_A],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_24_lte_b(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_24_LTE_B],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_24_openpgp(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_24_OPENPGP],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_24_os_9(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_24_OS_9],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_30_cdma(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_30_CDMA],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_31_philips(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_31_PHILIPS],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_32_aixm(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_32_AIXM],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_32_autosar(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_32_AUTOSAR],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_32_base91_d(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_32_BASE91_D],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_32_bzip2(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_32_BZIP2],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_32_cd_rom_edc(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_32_CD_ROM_EDC],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_32_cksum(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_32_CKSUM],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_32_iscsi(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_32_ISCSI],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_32_iso_hdlc(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_32_ISO_HDLC],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_32_jamcrc(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_32_JAMCRC],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_32_mef(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_32_MEF],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_32_mpeg_2(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_32_MPEG_2],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_32_xfer(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_32_XFER],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_40_gsm(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_40_GSM],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_64_ecma_182(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_64_ECMA_182],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_64_go_iso(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_64_GO_ISO],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_64_ms(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_64_MS],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_64_nvme(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_64_NVME],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_64_redis(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_64_REDIS],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_64_we(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_64_WE],data,n,&out);
  return out.lo;
}
uint64_t mulib_crc_64_xz(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_64_XZ],data,n,&out);
  return out.lo;
}
mulib_crc_value mulib_crc_82_darc(const void *data,size_t n)
{
  mulib_crc_value out={0,0};
  mulib_crc_compute(&mulib_crc_catalogue[MULIB_CRC_82_DARC],data,n,&out);
  return out;
}
#ifdef __cplusplus
}
#endif
#endif
