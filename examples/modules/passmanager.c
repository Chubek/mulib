#define MULIB_IMPL_PASSMANAGER
#include "mulib-passmanager.h"
#include <assert.h>
static int lower(void *program,void *context) {(void)context;*(int *)program=42;return 1;}
int main(void) {
    mulib_passmanager_t passes={0};passes.valid_analyses=7;int program=0;
    assert(mulib_passmanager_add(&passes,lower,NULL,3));
    assert(mulib_passmanager_run(&passes,&program,NULL)&&program==42&&passes.valid_analyses==3);
    mulib_passmanager_free(&passes);return 0;
}
