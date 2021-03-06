
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>

#include "vm.h"
#include "object.h"
#include "frame.h"
#include "symbol.h"
#include "jit-tracer.h"

hvm_call_trace *hvm_new_call_trace(hvm_vm *vm) {
  hvm_call_trace *trace = malloc(sizeof(hvm_call_trace));
  trace->entry = vm->ip;
  trace->sequence_capacity = HVM_TRACE_INITIAL_SEQUENCE_SIZE;
  trace->sequence_length = 0;
  trace->sequence = malloc(sizeof(hvm_trace_sequence_item) * trace->sequence_capacity);
  trace->complete = false;
  trace->caller_tag = NULL;
  trace->compiled_function = NULL;
  return trace;
}

void hvm_jit_call_trace_check_expand_capacity(hvm_call_trace *trace) {
  if(trace->sequence_length >= trace->sequence_capacity) {
    trace->sequence_capacity = 2 * trace->sequence_capacity;
    size_t size = sizeof(hvm_trace_sequence_item) * trace->sequence_capacity;
    trace->sequence = realloc(trace->sequence, size);
  }
}

hvm_trace_sequence_item *hvm_jit_call_trace_find_ip(hvm_call_trace *trace, uint64_t ip) {
  for(unsigned int i = 0; i < trace->sequence_length; i++) {
    hvm_trace_sequence_item *item = &trace->sequence[i];
    if(item->head.ip == ip) {
      return item;
    }
  }
  return NULL;
}

void hvm_jit_call_trace_push_instruction(hvm_vm *vm, hvm_call_trace *trace) {
  hvm_trace_sequence_item *item, *existing_item;

  // Skip tracing if we've already traced this instruction
  existing_item = hvm_jit_call_trace_find_ip(trace, vm->ip);
  if(existing_item != NULL) {
    trace->current_item = existing_item;
    return;
  }

  byte instr = vm->program[vm->ip];
  bool do_increment = true;
  // fprintf(stderr, "trace instruction: %d\n", instr);
  item = &trace->sequence[trace->sequence_length];
  // Keep track of the instruction the item originally came from
  item->head.ip = vm->ip;
  // Also note in the trace that this is the current item for our annotation
  // helpers.
  trace->current_item = item;

  switch(instr) {
    case HVM_OP_SETSTRING:
    case HVM_OP_SETSYMBOL:
      // TODO: Append a hvm_trace_sequence_item_setstring to our trace->sequence.
      if(instr == HVM_OP_SETSTRING) {
        item->setstring.head.type = HVM_TRACE_SEQUENCE_ITEM_SETSTRING;
      } else if(instr == HVM_OP_SETSYMBOL) {
        item->setsymbol.head.type = HVM_TRACE_SEQUENCE_ITEM_SETSYMBOL;
      }
      // 1B OP | 1B REG | 4B CONST
      item->setstring.register_return = vm->program[vm->ip + 1];
      item->setstring.constant        = *(uint32_t*)(&vm->program[vm->ip + 2]);
      break;

    case HVM_OP_INVOKEPRIMITIVE:
      item->invokeprimitive.head.type = HVM_TRACE_SEQUENCE_ITEM_INVOKEPRIMITIVE;
      item->invokeprimitive.register_symbol = vm->program[vm->ip + 1];
      item->invokeprimitive.register_return = vm->program[vm->ip + 2];
      // Look up the symbol of the primitive we're going to be invoking
      // TODO: Actually store the address of the register that's going to be
      //       read so that we can speed up checks in the future.
      hvm_obj_ref *ref = hvm_vm_register_read(vm, item->invokeprimitive.register_symbol);
      assert(ref->type == HVM_SYMBOL);
      item->invokeprimitive.symbol_value = ref->data.u64;
      break;

    case HVM_OP_RETURN:
      item->item_return.head.type = HVM_TRACE_SEQUENCE_ITEM_RETURN;
      item->item_return.register_return = vm->program[vm->ip + 1];
      // Look up the object being returned so we can annotate the trace
      // with its type.
      hvm_obj_ref *return_obj_ref = hvm_vm_register_read(vm, item->item_return.register_return);
      item->item_return.returning_type = return_obj_ref->type;
      // Mark this trace as complete
      trace->complete = true;
      // And register an index for it in the VM trace index
      unsigned short next_index = vm->traces_length + 1;
      // Make sure there's space for this trace
      assert(next_index < (HVM_MAX_TRACES - 1));
      vm->traces[next_index] = trace;
      // Update the caller's tag with the index if possible
      if(trace->caller_tag) {
        hvm_subroutine_tag tag;
        hvm_subroutine_read_tag(trace->caller_tag, &tag);
        // Actually setting the index here (remember it's off-by-one so that
        // 0 can mean not-set)
        tag.trace_index = next_index + 1;
        hvm_subroutine_write_tag(trace->caller_tag, &tag);
      }
      fprintf(stderr, "trace: completed trace %p\n", trace);
      break;

    case HVM_OP_IF:
      item->item_if.head.type = HVM_TRACE_SEQUENCE_ITEM_IF;
      item->item_if.register_value = vm->program[vm->ip + 1];
      item->item_if.destination    = *(uint64_t*)(&vm->program[vm->ip + 2]);
      item->item_if.branched       = false;
      break;

    case HVM_OP_GOTO:
      item->item_goto.head.type   = HVM_TRACE_SEQUENCE_ITEM_GOTO;
      item->item_goto.destination = *(uint64_t*)(&vm->program[vm->ip + 1]);
      break;

    case HVM_OP_ADD:
    case HVM_OP_EQ:
    case HVM_OP_GT:
    case HVM_OP_LT:
    case HVM_OP_AND:
      if(instr == HVM_OP_ADD) { item->head.type = HVM_TRACE_SEQUENCE_ITEM_ADD; }
      if(instr == HVM_OP_EQ)  { item->head.type = HVM_TRACE_SEQUENCE_ITEM_EQ;  }
      if(instr == HVM_OP_LT)  { item->head.type = HVM_TRACE_SEQUENCE_ITEM_LT;  }
      if(instr == HVM_OP_GT)  { item->head.type = HVM_TRACE_SEQUENCE_ITEM_GT;  }
      if(instr == HVM_OP_AND) { item->head.type = HVM_TRACE_SEQUENCE_ITEM_AND; }
      item->add.register_return   = vm->program[vm->ip + 1];
      item->add.register_operand1 = vm->program[vm->ip + 2];
      item->add.register_operand2 = vm->program[vm->ip + 3];
      break;

    case HVM_OP_ARRAYSET:
      item->arrayset.head.type = HVM_TRACE_SEQUENCE_ITEM_ARRAYSET;
      item->arrayset.register_array = vm->program[vm->ip + 1];
      item->arrayset.register_index = vm->program[vm->ip + 2];
      item->arrayset.register_value = vm->program[vm->ip + 3];
      break;

    case HVM_OP_ARRAYGET:
      item->arrayget.head.type = HVM_TRACE_SEQUENCE_ITEM_ARRAYGET;
      item->arrayget.register_return = vm->program[vm->ip + 1];
      item->arrayget.register_array = vm->program[vm->ip + 2];
      item->arrayget.register_index = vm->program[vm->ip + 3];
      break;

    case HVM_OP_ARRAYLEN:
      item->arraylen.head.type = HVM_TRACE_SEQUENCE_ITEM_ARRAYLEN;
      item->arraylen.register_return = vm->program[vm->ip + 1];
      item->arraylen.register_array  = vm->program[vm->ip + 2];
      break;

    case HVM_OP_ARRAYPUSH:
      item->arraypush.head.type = HVM_TRACE_SEQUENCE_ITEM_ARRAYPUSH;
      item->arraypush.register_array = vm->program[vm->ip + 1];
      item->arraypush.register_value = vm->program[vm->ip + 2];
      break;

    case HVM_OP_MOVE:
      item->move.head.type = HVM_TRACE_SEQUENCE_ITEM_MOVE;
      item->move.register_return = vm->program[vm->ip + 1];
      item->move.register_source = vm->program[vm->ip + 2];
      break;

    case HVM_OP_LITINTEGER:
      item->litinteger.head.type = HVM_TRACE_SEQUENCE_ITEM_LITINTEGER;
      item->litinteger.register_return = vm->program[vm->ip + 1];
      item->litinteger.literal_value   = *(int64_t*)(&vm->program[vm->ip + 2]);
      break;

    case HVM_OP_GETLOCAL:
      item->getlocal.head.type = HVM_TRACE_SEQUENCE_ITEM_GETLOCAL;
      item->getlocal.register_return = vm->program[vm->ip + 1];
      item->getlocal.register_symbol = vm->program[vm->ip + 2];
      break;

    case HVM_OP_SETLOCAL:
      item->setlocal.head.type = HVM_TRACE_SEQUENCE_ITEM_SETLOCAL;
      item->setlocal.register_symbol = vm->program[vm->ip + 1];
      item->setlocal.register_value  = vm->program[vm->ip + 2];
      break;

    case HVM_OP_GETGLOBAL:
      item->getglobal.head.type = HVM_TRACE_SEQUENCE_ITEM_GETGLOBAL;
      item->getglobal.register_return = vm->program[vm->ip + 1];
      item->getglobal.register_symbol = vm->program[vm->ip + 2];
      break;

    case HVM_OP_SETGLOBAL:
      item->setglobal.head.type = HVM_TRACE_SEQUENCE_ITEM_SETGLOBAL;
      item->setglobal.register_symbol = vm->program[vm->ip + 1];
      item->setglobal.register_value  = vm->program[vm->ip + 2];
      break;

    default:
      fprintf(stderr, "jit-tracer: Don't know what to do with instruction: %d\n", instr);
      do_increment = false;
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


hvm_trace_sequence_item *hvm_jit_tracer_get_current_item(hvm_vm *vm) {
  hvm_frame *frame              = vm->top;
  hvm_call_trace *trace         = frame->trace;
  hvm_trace_sequence_item *item = trace->current_item;
  return item;
}

void hvm_jit_tracer_annotate_invokeprimitive_returned_type(hvm_vm *vm, hvm_obj_ref *val) {
  hvm_frame *frame              = vm->top;
  hvm_call_trace *trace         = frame->trace;
  // Get the current item (was pushed in `hvm_jit_call_trace_push_instruction`)
  hvm_trace_sequence_item *item = trace->current_item;
  assert(item->head.type == HVM_TRACE_SEQUENCE_ITEM_INVOKEPRIMITIVE);
  // Update the return object type annotation using the object handed to us
  // by the VM.
  item->invokeprimitive.returned_type = val->type;
}

void hvm_jit_tracer_annotate_if_branched(hvm_vm *vm, bool branched) {
  hvm_frame *frame              = vm->top;
  hvm_call_trace *trace         = frame->trace;
  hvm_trace_sequence_item *item = trace->current_item;
  // Check to make sure the item is the right type and set the
  // `.branched` property.
  assert(item->head.type == HVM_TRACE_SEQUENCE_ITEM_IF);
  item->item_if.branched = branched;
}

void hvm_jit_tracer_annotate_getlocal(hvm_vm *vm, hvm_symbol_id symbol) {
  hvm_trace_sequence_item *item = hvm_jit_tracer_get_current_item(vm);
  assert(item->head.type == HVM_TRACE_SEQUENCE_ITEM_GETLOCAL);
  item->getlocal.symbol_value = symbol;
}

void hvm_jit_tracer_annotate_setlocal(hvm_vm *vm, hvm_symbol_id symbol) {
  hvm_trace_sequence_item *item = hvm_jit_tracer_get_current_item(vm);
  assert(item->head.type == HVM_TRACE_SEQUENCE_ITEM_SETLOCAL);
  item->setlocal.symbol_value = symbol;
}

void hvm_jit_tracer_dump_trace(hvm_vm *vm, hvm_call_trace *trace) {
  printf("  idx  ip          type\n");
  for(unsigned int i = 0; i < trace->sequence_length; i++) {
    hvm_trace_sequence_item *item = &trace->sequence[i];
    unsigned int idx = i;
    uint64_t address = item->head.ip;
    printf("  %-4d 0x%08llX  %-3d  ", idx, address, item->head.type);
    // Forward declarations for variables used in the big `switch`
    const char *type;
    byte reg, reg1, reg2, reg3;
    char *symbol_name;
    uint32_t short_symbol_id;
    uint64_t u64;
    int64_t  i64;

    switch(item->head.type) {
      case HVM_TRACE_SEQUENCE_ITEM_RETURN:
        type = hvm_human_name_for_obj_type(item->item_return.returning_type);
        printf("$%d.%s", item->item_return.register_return, type);
        break;
      case HVM_TRACE_SEQUENCE_ITEM_INVOKEPRIMITIVE:
        reg1 = item->invokeprimitive.register_return;
        reg2 = item->invokeprimitive.register_symbol;
        type = hvm_human_name_for_obj_type(item->invokeprimitive.returned_type);
        symbol_name = hvm_desymbolicate(vm->symbols, item->invokeprimitive.symbol_value);
        printf("$%-3d = invokeprimitive($%d = %s) -> %s", reg1, reg2, symbol_name, type);
        break;
      case HVM_TRACE_SEQUENCE_ITEM_ARRAYGET:
        reg1 = item->arrayset.register_array;
        reg2 = item->arrayset.register_index;
        reg3 = item->arrayset.register_value;
        printf("$%-3d = $%d.arrayget[$%d]", reg1, reg2, reg3);
        break;
      case HVM_TRACE_SEQUENCE_ITEM_ARRAYSET:
        reg1 = item->arrayset.register_array;
        reg2 = item->arrayset.register_index;
        reg3 = item->arrayset.register_value;
        printf("$%d.arrayset[$%d] = $%d", reg1, reg2, reg3);
        break;
      case HVM_TRACE_SEQUENCE_ITEM_SETSYMBOL:
        reg = item->setsymbol.register_return;
        short_symbol_id = item->setsymbol.constant;
        hvm_obj_ref *ref = hvm_const_pool_get_const(&vm->const_pool, short_symbol_id);
        symbol_name = hvm_desymbolicate(vm->symbols, ref->data.u64);
        printf("$%-3d = setsymbol(#%d = %s)", reg, short_symbol_id, symbol_name);
        break;
      case HVM_TRACE_SEQUENCE_ITEM_ADD:
        reg1 = item->add.register_return;
        reg2 = item->add.register_operand1;
        reg3 = item->add.register_operand2;
        printf("$%-3d = $%d + $%d", reg1, reg2, reg3);
        break;
      case HVM_TRACE_SEQUENCE_ITEM_IF:
        reg1 = item->item_if.register_value;
        u64  = item->item_if.destination;
        char *branched = (item->item_if.branched ? "yes" : "no");
        printf("if($%d, 0x%08llX, branched = %s)", reg1, u64, branched);
        break;
      case HVM_TRACE_SEQUENCE_ITEM_GOTO:
        u64 = item->item_goto.destination;
        printf("goto(0x%08llX)", u64);
        break;
      case HVM_TRACE_SEQUENCE_ITEM_ARRAYLEN:
        reg1 = item->arraylen.register_return;
        reg2 = item->arraylen.register_array;
        printf("$%-3d = $%d.arraylen", reg1, reg2);
        break;
      case HVM_TRACE_SEQUENCE_ITEM_LITINTEGER:
        reg1 = item->litinteger.register_return;
        i64  = item->litinteger.literal_value;
        printf("$%-3d = litinteger(%lld)", reg1, i64);
        break;
      case HVM_TRACE_SEQUENCE_ITEM_MOVE:
        reg1 = item->move.register_return;
        reg2 = item->move.register_source;
        printf("$%-3d = $%d", reg1, reg2);
        break;
      case HVM_TRACE_SEQUENCE_ITEM_EQ:
        reg1 = item->eq.register_return;
        reg2 = item->eq.register_operand1;
        reg3 = item->eq.register_operand2;
        char *cmp = "==";
        printf("$%-3d = $%-3d %s $%-3d", reg1, reg2, cmp, reg3);
        break;
      default:
        break;
    }

    printf("\n");
  }
}
