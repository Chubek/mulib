#ifndef ON1X_ARCHVIRT_RUNNER_H
#define ON1X_ARCHVIRT_RUNNER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Launch an On1x DomIOS guest with no host execution fallback. Returns the
 * guest's exit status, or 125 on launch, timeout, or protocol failure. This is
 * launcher support, not a general embedding or security-certification API. */
int on1x_archvirt_run_path(const char *guest_elf);

#ifdef __cplusplus
}
#endif
#endif
