#ifndef ON1X_ARCHVIRT_RUNNER_H
#define ON1X_ARCHVIRT_RUNNER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Launch an On1x DomIOS guest with no host execution fallback. Returns the
 * guest's exit status, or 125 on launch, timeout, or protocol failure. This is
 * launcher support, not a general embedding or security-certification API. */
int on1x_archvirt_run_path(const char *guest_elf);
int on1x_archvirt_check_path(const char *guest_elf);
/* Standalone runner only: expects a stdin pipe and temporarily owns SIGINT/SIGTERM. */
int on1x_archvirt_run_interactive_path(const char *guest_elf);
int on1x_archvirt_run_managed_path(const char *guest_elf, int ready_fd);
/* Private descriptor routes. The caller owns the fd and holds it open. */
int on1x_archvirt_check_sealed(int fd);
int on1x_archvirt_run_sealed(int fd);

#ifdef __cplusplus
}
#endif
#endif
