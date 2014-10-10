#ifndef HVM_JIT_COMPILER_H
#define HVM_JIT_COMPILER_H
/// @file jit-compiler.h

typedef struct hvm_trace_compiled_frame {
} hvm_trace_compiled_frame;

typedef enum {
  HVM_COMPILE_DATA_ARRAYSET
} hvm_compile_data_type;

#define HVM_COMPILE_DATA_HEAD hvm_compile_data_type type;

// Data item conventions:
//   .reg: Register that the result of the instruction goes in.

typedef struct hvm_compile_sequence_data_arrayset {
  HVM_COMPILE_DATA_HEAD;
  // Source-values for the data we're going to use to perform the array set
  // operation.
  LLVMValueRef array;
  LLVMValueRef index;
  LLVMValueRef value;
} hvm_compile_sequence_data_arrayset;

typedef struct hvm_compile_sequence_data_setsymbol {
  // Register that the `.symbol` would be placed into.
  byte reg;
  // The symbol that was retrieved up from the constant table.
  LLVMValueRef value;
  // Index into the constant table
  uint32_t constant;
} hvm_compile_sequence_data_setsymbol;

/// Structs used for figuring out and keeping track of data related to each
/// sequence in the trace instruction sequence being compiled.
typedef union hvm_compile_sequence_data {
  hvm_compile_sequence_data_arrayset  arrayset;
  hvm_compile_sequence_data_setsymbol setsymbol; 
} hvm_compile_sequence_data;

/// Holds all information relevant to a compilation of a trace (eg. instruction
/// sequence compilation data).
typedef struct hvm_compile_bundle {
  hvm_compile_sequence_data *data;
  // TODO: Keep track of registers and how they're being read and written.
} hvm_compile_bundle;

// External API
void hvm_jit_compile_trace(hvm_vm*, hvm_call_trace*);

// Compiler internals
void hvm_jit_compile_resolve_registers(hvm_vm*, hvm_call_trace*, hvm_compile_bundle*);

#endif
