#ifndef HVM_JIT_TRACER_H
#define HVM_JIT_TRACER_H
/// @file jit-tracer.h

/// Stores information about a call site (traces, JIT blocks, etc.).
typedef struct hvm_call_site {
  uint64_t ip;
} hvm_call_site;

/// Each trace of a subroutine call.
typedef struct hvm_call_trace {

} hvm_call_trace;

void hvm_jit_tracer_before_instruction(hvm_vm *vm);

#endif
