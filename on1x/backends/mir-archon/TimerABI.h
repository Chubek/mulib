#ifndef ON1X_ARCHON_TIMER_ABI_H
#define ON1X_ARCHON_TIMER_ABI_H

/* Private Archon scalar-register calling convention. A generated resume
 * receives [i64 state, i64 slots...] and writes one full i64 step kind. */
enum {
    ON1X_ARCHON_TIMER_SLEEP = 1,
    ON1X_ARCHON_TIMER_RETURN = 2,
    ON1X_ARCHON_TIMER_MAX_WORDS = 128
};

#endif
