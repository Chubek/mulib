#ifndef ON1X_GRAMMAREMBED_BRIDGE_H
#define ON1X_GRAMMAREMBED_BRIDGE_H
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Synchronous build-host bridge. Views are borrowed until the next compile.
 * No C++/liblr objects cross the ABI. Failure publishes no partial plan. */
bool on1x_grammar_compile(const char *source);
const char *on1x_grammar_plan(void);
const char *on1x_grammar_error(void);
const char *on1x_grammar_template(void);
const char *on1x_grammar_message(const char *key);
#ifdef __cplusplus
}
#endif
#endif
