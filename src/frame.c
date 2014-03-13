#include <stdlib.h>

#include "vm.h"
#include "symbol.h"
#include "object.h"
#include "frame.h"

hvm_frame *hvm_new_frame() {
  hvm_frame *frame = malloc(sizeof(hvm_frame));
  frame->return_addr = 0;
  frame->return_register = 0;
  frame->locals = hvm_new_obj_struct();
  return frame;
}
