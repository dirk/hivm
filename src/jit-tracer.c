
#include <stdlib.h>
#include <stdio.h>

#include "vm.h"
#include "object.h"
#include "frame.h"
#include "jit-tracer.h"

void hvm_jit_call_trace_push_instruction(hvm_vm *vm, hvm_call_trace *trace) {
  byte instr = vm->program[vm->ip];
  fprintf(stderr, "trace instruction: %d\n", instr);
}

void hvm_jit_tracer_before_instruction(hvm_vm *vm) {
  hvm_frame *frame = vm->top;
  //if(frame->trace) {
    hvm_call_trace *trace = frame->trace;
    hvm_jit_call_trace_push_instruction(vm, trace);
  //}
}
