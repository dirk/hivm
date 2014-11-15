#ifndef HVM_VM_H
#define HVM_VM_H
/// @file vm.h

#include <stdint.h>

#define HVM_PTR_TO_UINT64(P) *((uint64_t*)&P)

typedef unsigned char byte;

/// VM opcode (256 max)
typedef byte hvm_opcode;
/// VM instruction
typedef uint64_t hvm_instruction;

/// Internal ID of a symbol.
typedef uint64_t hvm_symbol_id;

/// Size of chunks to be allocated for storing bytecodes.
#define HVM_GENERATOR_GROW_RATE 65536

/// Initial size (in bytes) for program data.
/// @relates hvm_vm
#define HVM_PROGRAM_INITIAL_CAPACITY 16384

#define HVM_DEBUG_ENTRIES_INITIAL_CAPACITY 1024
#define HVM_DEBUG_ENTRIES_GROW_FUNCTION(V) (V * 2)

#define HVM_PROGRAM_GROW_FUNCTION(V) (V * 2)

/// Maximum stack size (in frames)
/// @relates hvm_vm
#define HVM_STACK_SIZE 16384

/// Threshold for a function to be hot and ready for tracing and compiling
#define HVM_TRACE_THRESHOLD 2

/// Maximum number of traces we can collect
#define HVM_MAX_TRACES 65535 // 2^16

extern struct hvm_obj_ref* hvm_const_null;

/// Generates bytecode.
/// @memberof hvm_generator
// void hvm_generate_bytecode(struct hvm_gen*);

/// @brief   Constant pools map an integer to a constant.
/// @details Can store approximately 4 billion constants (32-bit indexes).
typedef struct hvm_const_pool {
  /// Entries in the pool: array of pointers to object references.
  struct hvm_obj_ref** entries;
  /// Index of the next index for an entry to be inserted at (ie. the length).
  uint32_t next_index;
  /// Number of possible entries in the pool.
  uint32_t size;
} hvm_const_pool;

/// Start with 128 slots in the constant pool
/// @relates hvm_constant_pool
#define HVM_CONSTANT_POOL_INITIAL_SIZE 128
/// @relates hvm_constant_pool
#define HVM_CONSTANT_POOL_GROWTH_RATE  2

extern unsigned char HVM_DEBUG_FLAG_HIDE_BACKTRACE;

#define HVM_GENERAL_REGISTERS 128
#define HVM_ARGUMENT_REGISTERS 16
// One more than arg regs. Last one is $pn special reg.
#define HVM_PARAMETER_REGISTERS 17

#define HVM_REG_ZERO         128
#define HVM_REG_NULL         129
#define HVM_REG_ARG_OFFSET   130
#define HVM_REG_PARAM_OFFSET 146

bool hvm_is_gen_reg(byte i);
bool hvm_is_arg_reg(byte i);
bool hvm_is_param_reg(byte i);

byte hvm_vm_reg_gen(byte i);
byte hvm_vm_reg_zero();
byte hvm_vm_reg_null();
byte hvm_vm_reg_arg(byte i);
byte hvm_vm_reg_param(byte i);

/// Instance of the VM.
typedef struct hvm_vm {
  /// Root of call stack
  struct hvm_frame* root;
  /// Top of call stack (current execution frame)
  struct hvm_frame* top;
  /// Call stack
  struct hvm_frame* stack;
  /// Index of the current stack frame (total frames = stack_depth + 1)
  uint32_t stack_depth;

  // Current exception (NULL for no exception)
  struct hvm_exception *exception;
  // Debug entries
  struct hvm_chunk_debug_entry *debug_entries;
  uint64_t debug_entries_capacity;
  uint64_t debug_entries_size;

  /// Instruction pointer (indexes bytes in the program)
  uint64_t ip;
  /// Data for instructions
  byte* program;
  /// Amount of available data for instructions (in bytes)
  uint64_t program_capacity;
  /// Size of program memory (in bytes)
  uint64_t program_size;

  /// Pool of constants (dynamic array).
  hvm_const_pool const_pool;
  /// Symbol table: resolves symbols to code locations
  struct hvm_obj_struct *symbol_table;
  /// General purpose registers ($r0...$rN)
  struct hvm_obj_ref* general_regs[HVM_GENERAL_REGISTERS];
  struct hvm_obj_ref* arg_regs[HVM_ARGUMENT_REGISTERS];
  struct hvm_obj_ref* param_regs[HVM_PARAMETER_REGISTERS];

  /// Object space for the garbage collector
  struct hvm_gc1_obj_space *obj_space;

  /// VM-wide global variables
  struct hvm_obj_struct *globals;
  /// Symbol lookup
  struct hvm_symbol_store *symbols;
  /// Primitives
  struct hvm_obj_struct *primitives;

  /// Debugger information store
  void *debugger;

  /// Whether or not it's tracing
  bool is_tracing;
  /// Special flag to tell it to *always* trace
  bool always_trace;
  /// Array of traces that have been collected and are ready for compilation
  struct hvm_call_trace* traces[HVM_MAX_TRACES];
  /// Number of traces in .traces
  unsigned short traces_length;
} hvm_vm;

/// Create a new virtual machine.
/// @memberof hvm_vm
hvm_vm *hvm_new_vm();
/// Begin executing the virtual machine.
/// @memberof hvm_vm
void hvm_vm_run(hvm_vm*);

void hvm_vm_load_chunk(hvm_vm *vm, void *cv);

struct hvm_obj_ref *hvm_vm_register_read(hvm_vm *vm, byte reg);

/// Set a constant in the VM constant table.
/// @memberof hvm_vm
/// @param    vm
/// @param    id
/// @param    obj
/// @retval   hvm_obj_ref
void hvm_vm_set_const(hvm_vm *vm, uint32_t id, struct hvm_obj_ref* obj);
/// Get a constant.
/// @memberof hvm_vm
/// @param    vm
/// @param    id
/// @retval   hvm_obj_ref
struct hvm_obj_ref* hvm_vm_get_const(hvm_vm *vm, uint32_t id);

struct hvm_obj_ref* hvm_const_pool_get_const(hvm_const_pool*, uint32_t);
void hvm_const_pool_set_const(hvm_const_pool*, uint32_t, struct hvm_obj_ref*);
uint32_t hvm_vm_add_const(hvm_vm *vm, struct hvm_obj_ref* obj);


/// Get a local variable from a stack frame.
/// @memberof hvm_vm
struct hvm_obj_ref* hvm_get_local(struct hvm_frame*, hvm_symbol_id);
/// Set a local variable in a stack frame.
/// @memberof hvm_vm
void hvm_set_local(struct hvm_frame*, hvm_symbol_id, struct hvm_obj_ref*);


/// Get a global variable from the VM global struct (by symbol ID).
/// @memberof hvm_vm
struct hvm_obj_ref* hvm_get_global(hvm_vm*, hvm_symbol_id);
/// Set a global variable in the VM.
/// @memberof hvm_vm
void hvm_set_global(hvm_vm*, hvm_symbol_id, struct hvm_obj_ref*);

struct hvm_obj_ref *hvm_vm_call_primitive(hvm_vm*, struct hvm_obj_ref*);

struct hvm_obj_ref* hvm_vm_build_closure(hvm_vm *vm);

char *hvm_util_strclone(char *str);

/// Opcodes
typedef enum {
  HVM_OP_NOOP = 0,        // 1B OP
  HVM_OP_DIE  = 1,        // 1B OP
  HVM_OP_JUMP = 2,        // 1B OP | 4B DIFF
  HVM_OP_GOTO = 3,        // 1B OP | 8B DEST
  HVM_OP_RETURN = 4,      // 1B OP | 1B REG
  HVM_OP_IF = 5,          // 1B OP | 1B REG  | 8B DEST
  // Calls
  HVM_OP_CALL = 6,            // 1B OP | 3B TAG | 8B DEST  | 1B REG
  HVM_OP_CALLSYMBOLIC = 57,   // 1B OP | 3B TAG | 4B CONST | 1B REG
  HVM_OP_TAILCALL = 7,        // 1B OP | 3B TAG | 8B DEST
  // Invocations
  HVM_OP_INVOKESYMBOLIC = 8,  // 1B OP | 3B TAG | 1B REG   | 1B REG
  HVM_OP_INVOKEADDRESS = 9,   // 1B OP | 3B TAG | 1B REG   | 1B REG
  HVM_OP_INVOKEPRIMITIVE = 10,// 1B OP | 1B REG | 1B REG (untagged)

  HVM_OP_SETSTRING = 11, // 1B OP | 1B REG  | 4B CONST
  HVM_OP_SETINTEGER = 12,// 1B OP | 1B REG  | 4B CONST
  HVM_OP_SETFLOAT = 13,  // 1B OP | 1B REG  | 4B CONST
  HVM_OP_SETSTRUCT = 14, // 1B OP | 1B REG  | 4B CONST
  HVM_OP_SETSYMBOL = 15, // 1B OP | 1B REG  | 4B CONST
  HVM_OP_SETNULL = 16,   // 1B OP | 1B REG

  HVM_OP_LITINTEGER = 41,// 1B OP | 1B REG  | 8B LIT

  HVM_OP_GETLOCAL = 17,  // 1B OP | 1B REG  | 1B REG
  HVM_OP_SETLOCAL = 18,  // 1B OP | 1B REG  | 1B REG
  HVM_OP_GETGLOBAL = 19, // 1B OP | 1B REG  | 1B REG
  HVM_OP_SETGLOBAL = 20, // 1B OP | 1B REG  | 1B REG

  HVM_OP_GETCLOSURE = 56,   // 1B OP | 1B REG

  HVM_OP_ADD = 21,          // 1B OP | 3B REGs
  HVM_OP_SUB = 22,          // 1B OP | 3B REGs
  HVM_OP_MUL = 23,          // 1B OP | 3B REGs
  HVM_OP_DIV = 24,          // 1B OP | 3B REGs
  HVM_OP_MOD = 25,          // 1B OP | 3B REGs
  HVM_OP_POW = 26,          // 1B OP | 3B REGs

  HVM_OP_LT  = 45,          // 1B OP | 3B REGs
  HVM_OP_GT  = 46,          // 1B OP | 3B REGs
  HVM_OP_LTE = 47,          // 1B OP | 3B REGs
  HVM_OP_GTE = 48,          // 1B OP | 3B REGs
  HVM_OP_EQ  = 49,          // 1B OP | 3B REGs
  HVM_OP_AND = 60,          // 1B OP | 3B REGs

  // TODO: Bitwise instructions

  HVM_OP_CATCH = 50,            // 1B OP | 8B DEST | 1B REG
  HVM_OP_CLEARCATCH = 51,       // 1B OP
  HVM_OP_CLEAREXCEPTION = 52,   // 1B OP
  HVM_OP_SETEXCEPTION = 53,     // 1B OP | 1B REG
  HVM_OP_THROW = 54,            // 1B OP | 1B REG
  HVM_OP_GETEXCEPTIONDATA = 55, // 1B OP | 2B REG
  // TODO: RETHROW

  HVM_OP_ARRAYPUSH = 27,    // 1B OP | 2B REGS
  HVM_OP_ARRAYSHIFT = 28,   // 1B OP | 2B REGS
  HVM_OP_ARRAYPOP = 29,     // 1B OP | 2B REGS
  HVM_OP_ARRAYUNSHIFT = 30, // 1B OP | 2B REGS
  HVM_OP_ARRAYGET = 31,     // 1B OP | 3B REGS ( 1 = 2[3] )
  HVM_OP_ARRAYSET = 32,     // 1B OP | 3B REGS ( 1[2] = 3 )
  HVM_OP_ARRAYREMOVE = 33,  // 1B OP | 3B REGS
  HVM_OP_ARRAYNEW = 34,     // 1B OP | 2B REGS
  HVM_OP_ARRAYLEN = 35,     // 1B OP | 2B REGS ( 1 = length(2) )
  
  HVM_OP_STRUCTSET = 36,    // 1B OP | 3B REGS
  HVM_OP_STRUCTGET = 37,    // 1B OP | 3B REGS
  HVM_OP_STRUCTDELETE = 38, // 1B OP | 3B REGS
  HVM_OP_STRUCTNEW = 39,    // 1B OP | 1B REG
  HVM_OP_STRUCTHAS = 40,    // 1B OP | 3B REGS

  HVM_OP_MOVE = 43,// 1B OP | 1B REG | 1B REG
  HVM_OP_GOTOADDRESS = 44,// 1B OP | 1B REG

} hvm_opcodes;

/// Size of subroutine tags (in bytes)
#define HVM_SUBROUTINE_TAG_SIZE 3

// Struct for subroutine tags (3 bytes)
typedef struct hvm_subroutine_tag {
  unsigned short heat;// 8 bits (max is 255)
  unsigned short trace_index;// 16 bits
} hvm_subroutine_tag;

void hvm_subroutine_read_tag(byte *tag_start, hvm_subroutine_tag *tag);
void hvm_subroutine_write_tag(byte *tag_start, hvm_subroutine_tag *tag);

#endif
