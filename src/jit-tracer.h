#ifndef HVM_JIT_TRACER_H
#define HVM_JIT_TRACER_H
/// @file jit-tracer.h

typedef enum {
  HVM_TRACE_SEQUENCE_ITEM_SETSTRING,
  HVM_TRACE_SEQUENCE_ITEM_SETSYMBOL,
  HVM_TRACE_SEQUENCE_ITEM_INVOKEPRIMITIVE,
  HVM_TRACE_SEQUENCE_ITEM_RETURN,
  HVM_TRACE_SEQUENCE_ITEM_IF,
  HVM_TRACE_SEQUENCE_ITEM_GOTO,
  HVM_TRACE_SEQUENCE_ITEM_ADD,
  HVM_TRACE_SEQUENCE_ITEM_EQ,
  HVM_TRACE_SEQUENCE_ITEM_LT,
  HVM_TRACE_SEQUENCE_ITEM_GT,
  HVM_TRACE_SEQUENCE_ITEM_AND,
  HVM_TRACE_SEQUENCE_ITEM_ARRAYSET,
  HVM_TRACE_SEQUENCE_ITEM_ARRAYGET,
  HVM_TRACE_SEQUENCE_ITEM_MOVE,
  HVM_TRACE_SEQUENCE_ITEM_LITINTEGER,
  HVM_TRACE_SEQUENCE_ITEM_ARRAYLEN
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

// Copy SETSTRING for SETSYMBOL
typedef struct hvm_trace_sequence_item_setstring hvm_trace_sequence_item_setsymbol;

typedef struct hvm_trace_sequence_item_return {
  HVM_TRACE_SEQUENCE_ITEM_HEAD;
  /// Register we're returning from
  byte register_return;
  /// Type of the object being returned
  hvm_obj_type returning_type;
} hvm_trace_sequence_item_return;

typedef struct hvm_trace_sequence_item_invokeprimitive {
  HVM_TRACE_SEQUENCE_ITEM_HEAD;
  /// Register with the symbol for the primitive
  byte register_symbol;
  /// Register for the return value
  byte register_return;
  /// Type of the object returned from the primitive
  hvm_obj_type returned_type;
} hvm_trace_sequence_item_invokeprimitive;

typedef struct hvm_trace_sequence_item_if {
  HVM_TRACE_SEQUENCE_ITEM_HEAD;
  /// Register with the value we're checking
  byte register_value;
  /// Destination we're jumping to if true
  uint64_t destination;
} hvm_trace_sequence_item_if;

typedef struct hvm_trace_sequence_item_goto {
  HVM_TRACE_SEQUENCE_ITEM_HEAD;
  /// Destination we're jumping to
  uint64_t destination;
} hvm_trace_sequence_item_goto;

typedef struct hvm_trace_sequence_item_add {
  HVM_TRACE_SEQUENCE_ITEM_HEAD;
  byte register_result;
  byte register_operand1;
  byte register_operand2;
} hvm_trace_sequence_item_add;

// Equality check follows same format as add operation
typedef hvm_trace_sequence_item_add hvm_trace_sequence_item_eq;
typedef hvm_trace_sequence_item_add hvm_trace_sequence_item_and;
typedef hvm_trace_sequence_item_add hvm_trace_sequence_item_lt;
typedef hvm_trace_sequence_item_add hvm_trace_sequence_item_gt;

typedef struct hvm_trace_sequence_item_arrayset {
  HVM_TRACE_SEQUENCE_ITEM_HEAD;
  byte register_array;
  byte register_index;
  byte register_value;
} hvm_trace_sequence_item_arrayset;

typedef hvm_trace_sequence_item_arrayset hvm_trace_sequence_item_arrayget;

typedef struct hvm_trace_sequence_item_arraylen {
  HVM_TRACE_SEQUENCE_ITEM_HEAD;
  byte register_value;
  byte register_array;
} hvm_trace_sequence_item_arraylen;

typedef struct hvm_trace_sequence_item_move {
  HVM_TRACE_SEQUENCE_ITEM_HEAD;
  byte register_dest;
  byte register_source;
} hvm_trace_sequence_item_move;

typedef struct hvm_trace_sequence_item_litinteger {
  HVM_TRACE_SEQUENCE_ITEM_HEAD;
  byte register_value;
  int64_t literal_value;
} hvm_trace_sequence_item_litinteger;

typedef union hvm_trace_sequence_item {
  hvm_trace_sequence_item_setstring        setstring;
  hvm_trace_sequence_item_setsymbol        setsymbol;
  hvm_trace_sequence_item_invokeprimitive  invokeprimitive;
  hvm_trace_sequence_item_return           item_return;
  hvm_trace_sequence_item_if               item_if;
  hvm_trace_sequence_item_goto             item_goto;
  hvm_trace_sequence_item_add              add;
  hvm_trace_sequence_item_eq               eq;
  hvm_trace_sequence_item_lt               lt;
  hvm_trace_sequence_item_gt               gt;
  hvm_trace_sequence_item_and              and;
  hvm_trace_sequence_item_arrayset         arrayset;
  hvm_trace_sequence_item_arrayget         arrayget;
  hvm_trace_sequence_item_arraylen         arraylen;
  hvm_trace_sequence_item_move             move;
  hvm_trace_sequence_item_litinteger       litinteger;
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
  /// Whether or not the trace is done and ready for analysis
  bool complete;
} hvm_call_trace;

hvm_call_trace *hvm_new_call_trace(hvm_vm *vm);
void hvm_jit_tracer_before_instruction(hvm_vm *vm);

// Special hooks for annotating instructions (invoked by the JIT dispatcher)
void hvm_jit_tracer_annotate_invokeprimitive_returned_type(hvm_vm *vm, hvm_obj_ref *val);

#endif
