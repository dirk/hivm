#ifndef HVM_JIT_TRACER_H
#define HVM_JIT_TRACER_H
/// @file jit-tracer.h

typedef enum {
  HVM_TRACE_SEQUENCE_ITEM_SETSTRING
} hvm_trace_sequence_item_type;

#define HVM_TRACE_SEQUENCE_ITEM_HEAD hvm_trace_sequence_item_type type; \
                                     uint64_t ip;

typedef struct hvm_trace_sequence_item_setstring {
  HVM_TRACE_SEQUENCE_ITEM_HEAD;
  /// Destination register for the constant
  byte reg;
  /// Index into the constant table
  uint32_t constant;
} hvm_trace_sequence_item_setstring;

typedef union hvm_trace_sequence_item {
  hvm_trace_sequence_item_setstring setstring;
} hvm_trace_sequence_item;

/// Stores information about a call site (traces, JIT blocks, etc.).
typedef struct hvm_call_site {
  uint64_t ip;
} hvm_call_site;

/// Start traces off with space for 64 instructions.
#define HVM_TRACE_INITIAL_SEQUENCE_SIZE 64

/// Each trace of a subroutine call.
typedef struct hvm_call_trace {
  /// IP for the entry point of the trace.
  uint64_t entry;
  /// Meta-instruction sequence for the trace
  hvm_trace_sequence_item *sequence;
  /// Maximum of at least 4 billion instructions should be enough
  unsigned int sequence_length;
  /// Capacity of the sequence
  unsigned int sequence_capacity;
} hvm_call_trace;

hvm_call_trace *hvm_new_call_trace(hvm_vm *vm);
void hvm_jit_tracer_before_instruction(hvm_vm *vm);

#endif
