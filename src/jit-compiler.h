#ifndef HVM_JIT_COMPILER_H
#define HVM_JIT_COMPILER_H
/// @file jit-compiler.h

// Define some LLVM pointer types if LLVM isn't present to define them for us.
#ifndef LLVM_C_CORE_H
typedef void* LLVMBasicBlockRef;
typedef void* LLVMBuilderRef;
typedef void* LLVMValueRef;
#endif

typedef struct hvm_trace_compiled_frame {
} hvm_trace_compiled_frame;

typedef struct hvm_jit_block {
  /// IP in the VM where this block begins
  uint64_t ip;
  /// LLVM BasicBlock itself
  LLVMBasicBlockRef basic_block;
  /// Next block in the linked list
  struct hvm_jit_block *next;
} hvm_jit_block;

typedef enum {
  HVM_COMPILE_DATA_ARRAYSET,
  HVM_COMPILE_DATA_ARRAYGET,
  HVM_COMPILE_DATA_ARRAYLEN,
  HVM_COMPILE_DATA_ADD,
  HVM_COMPILE_DATA_SETSYMBOL,
  HVM_COMPILE_DATA_SETSTRING,
  HVM_COMPILE_DATA_INVOKEPRIMITIVE,
  HVM_COMPILE_DATA_IF,
  HVM_COMPILE_DATA_GOTO,
  HVM_COMPILE_DATA_LITINTEGER,
  HVM_COMPILE_DATA_MOVE,
  HVM_COMPILE_DATA_EQ,
  HVM_COMPILE_DATA_GT,
  HVM_COMPILE_DATA_AND,
  HVM_COMPILE_DATA_RETURN,
  HVM_COMPILE_DATA_SETLOCAL,
  HVM_COMPILE_DATA_GETLOCAL
} hvm_compile_data_type;

#define HVM_COMPILE_DATA_HEAD hvm_compile_data_type type;

#define HVM_UNKNOWN_TYPE -1

/// Wrapped values provide meta-data storage around a particular value. These
/// will allow us to track information around that value to do analysis
/// and optimizations based on it.
typedef struct hvm_compile_value {
  /// Register this value is located in.
  byte reg;
  /// Type of the value being referenced.
  char type;
  /// If we know that this will be a constant.
  bool constant;
  /// Constant object (this is the value contained in .constant_value)
  hvm_obj_ref *constant_object;
  // #ifdef LLVM_C_CORE_H
  //   /// Reference to the LLVM value pointing to the constant object.
  //   LLVMValueRef constant_value;
  // #else
  //   void *constant_value;
  // #endif
} hvm_compile_value;


// Data item conventions:
//   .register_return: Register that the result of the instruction goes in.

typedef struct hvm_compile_sequence_data_head {
  HVM_COMPILE_DATA_HEAD;
} hvm_compile_sequence_data_head;

typedef struct hvm_compile_sequence_data_move {
  HVM_COMPILE_DATA_HEAD;
  /// Register value will be copied to
  byte register_return;
  /// Value that will be in the register
  hvm_compile_value *value;
} hvm_compile_sequence_data_move;

typedef struct hvm_compile_sequence_data_goto {
  HVM_COMPILE_DATA_HEAD;
  hvm_jit_block *destination_block;
} hvm_compile_sequence_data_goto;

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
  byte register_return;
} hvm_compile_sequence_data_arrayget;

typedef struct hvm_compile_sequence_data_add {
  HVM_COMPILE_DATA_HEAD;
  LLVMValueRef operand1;
  LLVMValueRef operand2;
  // Results
  LLVMValueRef result;
  byte register_return;
} hvm_compile_sequence_data_add;

typedef struct hvm_compile_sequence_data_if {
  HVM_COMPILE_DATA_HEAD;
  hvm_jit_block *falsey_block;
  hvm_jit_block *truthy_block;
} hvm_compile_sequence_data_if;

typedef struct hvm_compile_sequence_data_setsymbol {
  HVM_COMPILE_DATA_HEAD;
  // Register that the `.symbol` would be placed into
  byte register_return;
  // The symbol that was retrieved up from the constant table
  LLVMValueRef value;
  // Index into the constant table
  uint32_t constant;
} hvm_compile_sequence_data_setsymbol;

typedef struct hvm_compile_sequence_data_setsymbol hvm_compile_sequence_data_setstring;

typedef struct hvm_compile_sequence_data_litinteger {
  HVM_COMPILE_DATA_HEAD;
  // Register that the integer will be put in
  byte register_return;
  // The constant integer in LLVM
  LLVMValueRef value;
} hvm_compile_sequence_data_litinteger;

typedef struct hvm_compile_sequence_data_invokeprimitive {
  HVM_COMPILE_DATA_HEAD;
  // Source registers for the symbol ID
  byte register_symbol;
  // Value for the symbol ID for `hvm_vm_call_primitive(vm, symbol_id)`
  hvm_symbol_id symbol_id;
  LLVMValueRef  symbol_value;
  // Result value and register
  LLVMValueRef  value;
  byte          register_return;
} hvm_compile_sequence_data_invokeprimitive;

typedef struct hvm_compile_sequence_data_getlocal {
  HVM_COMPILE_DATA_HEAD;
  // Slot allocated for this local variable; set up by
  // `hvm_jit_compile_pass_identify_locals`
  LLVMValueRef slot;
} hvm_compile_sequence_data_getlocal;

typedef struct hvm_compile_sequence_data_setlocal {
  HVM_COMPILE_DATA_HEAD;
  LLVMValueRef slot;
} hvm_compile_sequence_data_setlocal;

/// Structs used for figuring out and keeping track of data related to each
/// sequence in the trace instruction sequence being compiled.
typedef union hvm_compile_sequence_data {
  hvm_compile_sequence_data_head       head;
  hvm_compile_sequence_data_move       move;
  hvm_compile_sequence_data_arrayset   arrayset;
  hvm_compile_sequence_data_arrayget   arrayget;
  hvm_compile_sequence_data_add        add;
  hvm_compile_sequence_data_setsymbol  setsymbol;
  hvm_compile_sequence_data_setstring  setstring;
  hvm_compile_sequence_data_if         item_if;
  hvm_compile_sequence_data_goto       item_goto;
  hvm_compile_sequence_data_litinteger litinteger;
  hvm_compile_sequence_data_getlocal   getlocal;
  hvm_compile_sequence_data_setlocal   setlocal;
  hvm_compile_sequence_data_invokeprimitive invokeprimitive;
} hvm_compile_sequence_data;

/// Holds all information relevant to a compilation of a trace (eg. instruction
/// sequence compilation data).
typedef struct hvm_compile_bundle {
  // VM execution frame
  struct hvm_frame *frame;
  // Instructions as they are compiled
  hvm_compile_sequence_data *data;

  /// Head of linked list of blocks
  hvm_jit_block *blocks_head;
  hvm_jit_block *blocks_tail;
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
  LLVMValueRef           llvm_function;
#endif
} hvm_compile_bundle;

/// Used by the compilation passes to share state and information about the
/// code being compiled.
struct hvm_jit_compile_context {
  /// Bundle of LLVM objects and such
  hvm_compile_bundle *bundle;
  /// Boxes for object references (emulating the virtual registers in the VM),
  /// however LLVM will optimize our stores and loads to/from these into faster
  /// phi nodes
  LLVMValueRef *general_regs;
  /// For knowing whether a register is constant or not
  bool *constant_regs;
  /// Local variable slots
  struct hvm_obj_struct *locals;
  /// Pointer to the VM we're compiling for
  hvm_vm *vm;
  /// Boxes for wrapped values corresponding to a register.
  hvm_compile_value **values;
};


typedef enum {
  HVM_JIT_EXIT_BAILOUT,
  HVM_JIT_EXIT_RETURN
} hvm_jit_exit_status;

/// Exited with a bailout; VM will resume execution at `destination`.
typedef struct hvm_jit_exit_bailout {
  hvm_jit_exit_status  status;
  uint64_t             destination;
} hvm_jit_exit_bailout;

/// JITed function reached a return statement successfully. Object returned
/// is in `value`.
typedef struct hvm_jit_exit_return {
  hvm_jit_exit_status  status;
  hvm_obj_ref         *value;
} hvm_jit_exit_return;

typedef union hvm_jit_exit {
  hvm_jit_exit_bailout bailout;
  hvm_jit_exit_return  ret;
} hvm_jit_exit;

// Second argument is the pointer to the first element of the parameter
// registers array.
typedef void (*hvm_jit_native_function)(hvm_jit_exit*, hvm_obj_ref*);

// External API

/// Compile a given trace to native function.
void hvm_jit_compile_trace(hvm_vm*, hvm_call_trace*);
/// Run a compiled traced.
hvm_jit_exit* hvm_jit_run_compiled_trace(hvm_vm*, hvm_call_trace*);

/// Given a trace and a compilation bundle, actually compiles each item
/// in the trace into the LLVM IR builder.
void hvm_jit_compile_builder(hvm_vm*, hvm_call_trace*, hvm_compile_bundle*);

/// Given a certain instruction address (that's the destination for a
/// branch/jump somewhere in the block), finds an already-inserted LLVM IR
/// basic-block for that address or creates-and-inserts a new one.
hvm_jit_block* hvm_jit_compile_find_or_insert_block(LLVMValueRef, hvm_compile_bundle*, uint64_t);

/// Set up a bailout from the current JIT state back to the normal VM state
/// at the given instruction address; execution will resume at that address
/// in the VM.
LLVMBasicBlockRef hvm_jit_build_bailout_block(hvm_vm*, LLVMBuilderRef, LLVMValueRef parent_func, LLVMValueRef exit_value, void*, uint64_t);

#endif
