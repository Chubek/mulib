#ifndef ON1X_CONTRACT_STRINGS_H
#define ON1X_CONTRACT_STRINGS_H
/* ABI fixture only; review flags in tests are synthetic, not certification. */
#ifdef __cplusplus
extern "C" {
#endif
long long contract_text_size(const char *text);
const char *contract_static_text(int which);
void contract_text_note(const char *first, const char *second);
#ifdef __cplusplus
}
#endif
#endif
