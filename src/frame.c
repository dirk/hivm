#include <stdlib.h>

#include "vm.h"
#include "symbol.h"
#include "object.h"
#include "frame.h"

hvm_frame *hvm_new_frame() {
  hvm_frame *frame = malloc(sizeof(hvm_frame));
  frame->current_addr = 0;
  frame->return_addr = 0;
  frame->return_register = 0;
  frame->locals = hvm_new_obj_struct();
  return frame;
}

hvm_location *hvm_new_location() {
  hvm_location *loc = malloc(sizeof(hvm_location));
  loc->frame = NULL;
  loc->name  = NULL;
  loc->file  = NULL;
  loc->line  = 0;
  return loc;
}
