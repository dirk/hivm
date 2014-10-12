#ifndef HVM_JIT_COMPILER_H
#define HVM_JIT_COMPILER_H
/// @file jit-compiler.h

typedef struct hvm_trace_compiled_frame {
} hvm_trace_compiled_frame;

typedef enum {
  HVM_COMPILE_DATA_ARRAYSET,
  HVM_COMPILE_DATA_ARRAYGET,
  HVM_COMPILE_DATA_ADD,
  HVM_COMPILE_DATA_SETSYMBOL,
  HVM_COMPILE_DATA_INVOKEPRIMITIVE
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

typedef struct hvm_compile_sequence_data_arrayget {
  HVM_COMPILE_DATA_HEAD;
  // Sourcing values (array and the index into the array)
  LLVMValueRef array;
  LLVMValueRef index;
  // Result value and result register
  LLVMValueRef value;
  byte reg;
} hvm_compile_sequence_data_arrayget;

typedef struct hvm_compile_sequence_data_add {
  HVM_COMPILE_DATA_HEAD;
  LLVMValueRef operand1;
  LLVMValueRef operand2;
  // Results
  LLVMValueRef result;
  byte reg;
} hvm_compile_sequence_data_add;

typedef struct hvm_compile_sequence_data_if {
  HVM_COMPILE_DATA_HEAD;
} hvm_compile_sequence_data_if;

typedef struct hvm_compile_sequence_data_setsymbol {
  HVM_COMPILE_DATA_HEAD;
  // Register that the `.symbol` would be placed into.
  byte reg;
  // The symbol that was retrieved up from the constant table.
  LLVMValueRef value;
  // Index into the constant table
  uint32_t constant;
} hvm_compile_sequence_data_setsymbol;

typedef struct hvm_compile_sequence_data_invokeprimitive {
  HVM_COMPILE_DATA_HEAD;
  // Source registers for the symbol ID
  byte register_symbol;
  // Value for the symbol ID for `hvm_vm_call_primitive(vm, symbol_id)`
  hvm_symbol_id symbol_id;
  LLVMValueRef  symbol_value;
  // Result value and register
  LLVMValueRef  value;
  byte          reg;
} hvm_compile_sequence_data_invokeprimitive;

/// Structs used for figuring out and keeping track of data related to each
/// sequence in the trace instruction sequence being compiled.
typedef union hvm_compile_sequence_data {
  hvm_compile_sequence_data_arrayset  arrayset;
  hvm_compile_sequence_data_arrayget  arrayget;
  hvm_compile_sequence_data_add       add;
  hvm_compile_sequence_data_setsymbol setsymbol;
  hvm_compile_sequence_data_invokeprimitive invokeprimitive;
} hvm_compile_sequence_data;

typedef struct hvm_jit_block {
  // The IP in the VM
  uint64_t ip;
  // The original index in the trace sequence that the block is for.
  unsigned int index;
  // The LLVM BasicBlock itself
  LLVMBasicBlockRef basic_block;
} hvm_jit_block;

/// Holds all information relevant to a compilation of a trace (eg. instruction
/// sequence compilation data).
typedef struct hvm_compile_bundle {
  hvm_compile_sequence_data *data;

  hvm_jit_block *blocks;
  unsigned int   blocks_length;
  // TODO: Keep track of registers and how they're being read and written.
  // void *llvm_module;
  // void *llvm_engine;
  // void *llvm_builder;
#ifdef LLVM_C_CORE_H
  // Full LLVM types
  LLVMModuleRef          llvm_module;
  LLVMExecutionEngineRef llvm_engine;
  LLVMBuilderRef         llvm_builder;
#endif
} hvm_compile_bundle;

// External API
void hvm_jit_compile_trace(hvm_vm*, hvm_call_trace*);

// Compiler internals
void hvm_jit_compile_resolve_registers(hvm_vm*, hvm_call_trace*, hvm_compile_bundle*);

#endif
