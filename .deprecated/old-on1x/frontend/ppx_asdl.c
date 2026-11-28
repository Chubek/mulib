#include "ppx_ast.h"
#include <on1x/generated_ppx.h>
void *on1x_ppx_arena_new(void) {
 on1xppx_arena *arena=malloc(sizeof(*arena));
 if(arena)on1xppx_arena_init(arena);
 return arena;
}
void on1x_ppx_arena_free(void *value) {
 on1xppx_arena *arena=value;
 if(arena){on1xppx_arena_destroy(arena);free(arena);}
}
void *on1x_ppx_atom(void *arena,int quoted,const char *text,size_t length) {
 on1xppx_string_t value=on1xppx_string_copy(arena,text,length);
 if(!value.data)return NULL;
 return quoted?on1xppx_Text(arena,value):on1xppx_Symbol(arena,value);
}
void *on1x_ppx_list(void *arena,void *const *items,size_t length) {
 on1xppx_wire_seq sequence=on1xppx_wire_seq_new(arena,length);
 if(length&&!sequence.items)return NULL;
 for(size_t i=0;i<length;++i)sequence.items[i]=items[i];
 return on1xppx_List(arena,sequence);
}
int on1x_ppx_validate(void *node) {return on1xppx_validate_wire(node,257);}
