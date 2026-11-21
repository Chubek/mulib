#include "On1xVM.h"

int On1xVM_tape_validate(const On1xVM_tape *tape) {
  size_t i;
  if(!tape || tape->size>tape->capacity || (tape->size && !tape->data)) return -1;
  for(i=0;i<tape->size;++i) {
    if(On1xVM_vw_validate_instruction(&tape->data[i],tape->size)) return -4;
  }
  return 0;
}
