
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

bool hvm_jit_call_trace_contains_ip(hvm_call_trace *trace, uint64_t ip) {
  for(unsigned int i = 0; i < trace->sequence_length; i++) {
    hvm_trace_sequence_item *item = &trace->sequence[i];
    if(item->head.ip == ip) {
      return true;
    }
  }
  return false;
}

void hvm_jit_call_trace_push_instruction(hvm_vm *vm, hvm_call_trace *trace) {
  // Skip tracing if we've already traced this instruction
  if(hvm_jit_call_trace_contains_ip(trace, vm->ip)) {
    return;
  }
  byte instr = vm->program[vm->ip];
  bool do_increment = true;
  // fprintf(stderr, "trace instruction: %d\n", instr);
  hvm_trace_sequence_item *item = &trace->sequence[trace->sequence_length];
  // Keep track of the instruction the item originally came from
  item->head.ip = vm->ip;

  switch(instr) {
    case HVM_OP_SETSTRING:
    case HVM_OP_SETSYMBOL:
      // TODO: Append a hvm_trace_sequence_item_setstring to our trace->sequence.
      if(instr == HVM_OP_SETSTRING) {
        item->setstring.type = HVM_TRACE_SEQUENCE_ITEM_SETSTRING;
      } else if(instr == HVM_OP_SETSYMBOL) {
        item->setsymbol.type = HVM_TRACE_SEQUENCE_ITEM_SETSYMBOL;
      }
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
      item->item_return.type = HVM_TRACE_SEQUENCE_ITEM_RETURN;
      item->item_return.register_return = vm->program[vm->ip + 1];
      // Look up the object being returned so we can annotate the trace
      // with its type.
      hvm_obj_ref *return_obj_ref = hvm_vm_register_read(vm, item->item_return.register_return);
      item->item_return.returning_type = return_obj_ref->type;
      // Mark this trace as complete
      trace->complete = true;
      fprintf(stderr, "trace: completed trace %p\n", trace);
      break;

    case HVM_OP_IF:
      item->item_if.type = HVM_TRACE_SEQUENCE_ITEM_IF;
      item->item_if.register_value = vm->program[vm->ip + 1];
      item->item_if.destination    = *(uint64_t*)(&vm->program[vm->ip + 2]);
      break;

    case HVM_OP_GOTO:
      item->item_goto.type = HVM_TRACE_SEQUENCE_ITEM_GOTO;
      item->item_goto.destination = *(uint64_t*)(&vm->program[vm->ip + 2]);
      break;

    case HVM_OP_ADD:
    case HVM_OP_EQ:
    case HVM_OP_AND:
    case HVM_OP_GT:
    case HVM_OP_LT:
      if(instr == HVM_OP_ADD) { item->add.type = HVM_TRACE_SEQUENCE_ITEM_ADD; }
      if(instr == HVM_OP_EQ)  { item->eq.type  = HVM_TRACE_SEQUENCE_ITEM_EQ;  }
      if(instr == HVM_OP_LT)  { item->eq.type  = HVM_TRACE_SEQUENCE_ITEM_LT;  }
      if(instr == HVM_OP_GT)  { item->eq.type  = HVM_TRACE_SEQUENCE_ITEM_GT;  }
      if(instr == HVM_OP_AND) { item->and.type = HVM_TRACE_SEQUENCE_ITEM_AND; }
      item->add.register_result   = vm->program[vm->ip + 1];
      item->add.register_operand1 = vm->program[vm->ip + 2];
      item->add.register_operand2 = vm->program[vm->ip + 3];
      break;

    case HVM_OP_ARRAYSET:
      item->arrayset.type = HVM_TRACE_SEQUENCE_ITEM_ARRAYSET;
      item->arrayset.register_array = vm->program[vm->ip + 1];
      item->arrayset.register_index = vm->program[vm->ip + 2];
      item->arrayset.register_value = vm->program[vm->ip + 3];
      break;

    case HVM_OP_ARRAYGET:
      item->arrayget.type = HVM_TRACE_SEQUENCE_ITEM_ARRAYGET;
      item->arrayget.register_value = vm->program[vm->ip + 1];
      item->arrayget.register_array = vm->program[vm->ip + 2];
      item->arrayget.register_index = vm->program[vm->ip + 3];
      break;

    case HVM_OP_ARRAYLEN:
      item->arraylen.type = HVM_TRACE_SEQUENCE_ITEM_ARRAYLEN;
      item->arraylen.register_value = vm->program[vm->ip + 1];
      item->arraylen.register_array = vm->program[vm->ip + 2];
      break;      

    case HVM_OP_MOVE:
      item->move.type = HVM_TRACE_SEQUENCE_ITEM_MOVE;
      item->move.register_dest   = vm->program[vm->ip + 1];
      item->move.register_source = vm->program[vm->ip + 2];
      break;

    case HVM_OP_LITINTEGER:
      item->litinteger.type = HVM_TRACE_SEQUENCE_ITEM_LITINTEGER;
      item->litinteger.register_value = vm->program[vm->ip + 1];
      item->litinteger.literal_value  = *(int64_t*)(&vm->program[vm->ip + 2]);
      break;

    default:
      fprintf(stderr, "trace: don't know what to do with instruction: %d\n", instr);
      do_increment = 0;
  }
  if(do_increment == true) {
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

void hvm_jit_tracer_dump_trace(hvm_call_trace *trace) {
  printf("  idx  ip\n");
  for(unsigned int i = 0; i < trace->sequence_length; i++) {
    hvm_trace_sequence_item *item = &trace->sequence[i];
    unsigned int idx = i;
    uint64_t address = item->head.ip;
    printf("  %-4d 0x%08llX  ", idx, address);
    // Dump the item's actual information
    printf("\n");
  }
}
