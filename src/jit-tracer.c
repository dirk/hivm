
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>

#include "vm.h"
#include "object.h"
#include "frame.h"
#include "jit-tracer.h"

hvm_call_trace *hvm_new_call_trace(hvm_vm *vm) {
  hvm_call_trace *trace = malloc(sizeof(hvm_call_trace));
  trace->entry = vm->ip;
  trace->sequence_capacity = HVM_TRACE_INITIAL_SEQUENCE_SIZE;
  trace->sequence_length = 0;
  trace->sequence = malloc(sizeof(hvm_trace_sequence_item) * trace->sequence_capacity);
  trace->complete = false;
  return trace;
}

void hvm_jit_call_trace_check_expand_capacity(hvm_call_trace *trace) {
  if(trace->sequence_length >= trace->sequence_capacity) {
    trace->sequence_capacity = 2 * trace->sequence_capacity;
    size_t size = sizeof(hvm_trace_sequence_item) * trace->sequence_capacity;
    trace->sequence = realloc(trace->sequence, size);
  }
}

void hvm_jit_call_trace_push_instruction(hvm_vm *vm, hvm_call_trace *trace) {
  byte instr = vm->program[vm->ip];
  char do_increment = 1;
  // fprintf(stderr, "trace instruction: %d\n", instr);
  hvm_trace_sequence_item *item = &trace->sequence[trace->sequence_length];
  switch(instr) {
    case HVM_OP_SETSTRING:
    case HVM_OP_SETSYMBOL:
      // TODO: Append a hvm_trace_sequence_item_setstring to our trace->sequence.
      if(instr == HVM_OP_SETSTRING) {
        item->setstring.type = HVM_TRACE_SEQUENCE_ITEM_SETSTRING;
      } else if(instr == HVM_OP_SETSYMBOL) {
        item->setsymbol.type = HVM_TRACE_SEQUENCE_ITEM_SETSYMBOL;
      }
      item->setstring.ip   = vm->ip;
      // 1B OP | 1B REG | 4B CONST
      item->setstring.reg      = vm->program[vm->ip + 1];
      item->setstring.constant = *(uint32_t*)(&vm->program[vm->ip + 2]);
      break;

    case HVM_OP_INVOKEPRIMITIVE:
      item->invokeprimitive.type = HVM_TRACE_SEQUENCE_ITEM_INVOKEPRIMITIVE;
      item->invokeprimitive.register_symbol = vm->program[vm->ip + 1];
      item->invokeprimitive.register_return = vm->program[vm->ip + 2];
      break;

    case HVM_OP_RETURN:
      item->_return.type = HVM_TRACE_SEQUENCE_ITEM_RETURN;
      item->_return.register_return = vm->program[vm->ip + 1];
      // Look up the object being returned so we can annotate the trace
      // with its type.
      hvm_obj_ref *return_obj_ref = hvm_vm_register_read(vm, item->_return.register_return);
      item->_return.returning_type = return_obj_ref->type;
      // Mark this trace as complete
      trace->complete = true;
      fprintf(stderr, "trace: completed trace %p\n", trace);
      break;

    default:
      fprintf(stderr, "trace: don't know what to do with instruction: %d\n", instr);
      do_increment = 0;
  }
  if(do_increment == 1) {
    trace->sequence_length += 1;
    hvm_jit_call_trace_check_expand_capacity(trace);
  }
}

void hvm_jit_tracer_before_instruction(hvm_vm *vm) {
  hvm_frame *frame = vm->top;
  if(frame->trace != NULL) {
    hvm_call_trace *trace = frame->trace;
    hvm_jit_call_trace_push_instruction(vm, trace);
  }
}

void hvm_jit_tracer_annotate_invokeprimitive_returned_type(hvm_vm *vm, hvm_obj_ref *val) {
  hvm_frame *frame = vm->top;
  hvm_call_trace *trace = frame->trace;
  // Get the current item (was pushed in `hvm_jit_call_trace_push_instruction`)
  hvm_trace_sequence_item *item = &trace->sequence[trace->sequence_length - 1];
  assert(item->invokeprimitive.type == HVM_TRACE_SEQUENCE_ITEM_INVOKEPRIMITIVE);
  // Update the return object type annotation using the object handed to us
  // by the VM.
  item->invokeprimitive.returned_type = val->type;
}
