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
#define HVM_PROGRAM_INITIAL_SIZE 16384

/// Maximum stack size (in frames)
/// @relates hvm_vm
#define HVM_STACK_SIZE 16384

extern struct hvm_obj_ref* hvm_const_null;

/// @brief Chunk of instruction code and data (constants, etc.).
typedef struct hvm_chunk {

} hvm_chunk;

/// @brief Stores instructions, constants, etc. for a chunk. Can then generate the
///        appropriate bytecode for that chunk.
typedef struct hvm_generator {
  // nothing
} hvm_generator;

/// Generates bytecode.
/// @memberof hvm_generator
void hvm_generator_bytecode(hvm_generator*);

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

#define HVM_GENERAL_REGISTERS 128

/// Instance of the VM.
typedef struct hvm_vm {
  /// Root of call stack
  struct hvm_frame* root;
  /// Top of call stack (current execution frame)
  struct hvm_frame* top;
  /// Call stack
  struct hvm_frame** stack;

  /// Instruction pointer (indexes bytes in the program)
  uint64_t ip;
  /// Data for instructions
  byte* program;
  /// Size of program memory (in bytes)
  uint64_t program_size;

  /// Pool of constants (dynamic array).
  hvm_const_pool const_pool;
  /// General purpose registers ($r0...$rN)
  struct hvm_obj_ref* general_regs[HVM_GENERAL_REGISTERS];
  // heap
  // object space
  /// VM-wide global variables
  struct hvm_obj_struct *globals;
} hvm_vm;

/// Create a new virtual machine.
/// @memberof hvm_vm
hvm_vm *hvm_new_vm();
/// Begin executing the virtual machine.
/// @memberof hvm_vm
void hvm_vm_run(hvm_vm*);

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

/// Opcodes
typedef enum {
  HVM_OP_NOOP = 0,       // 1B OP
  HVM_OP_DIE  = 1,       // 1B OP
  HVM_OP_JUMP = 3,       // 1B OP | 4B DIFF
  HVM_OP_GOTO = 2,       // 1B OP | 8B DEST
  HVM_OP_CALL = 4,       // 1B OP | 8B DEST | 1B REG
  HVM_OP_TAILCALL = 5,   // 1B OP | 8B DEST
  HVM_OP_RETURN = 6,     // 1B OP | 1B REG
  HVM_OP_IF = 7,         // 1B OP | 1B REG  | 8B DEST

  HVM_OP_SETSTRING = 3,  // 1B OP | 1B REG  | 4B CONST
  HVM_OP_SETINTEGER = 4, // 1B OP | 1B REG  | 4B CONST
  HVM_OP_SETFLOAT = 5,   // 1B OP | 1B REG  | 4B CONST
  HVM_OP_SETSTRUCT = 6,  // 1B OP | 1B REG  | 4B CONST
  HVM_OP_SETNULL = 7,    // 1B OP | 1B REG

  HVM_OP_SETLOCAL = 15,  // 1B OP | 4B SYM  | 1B REG
  HVM_OP_GETLOCAL = 16,  // 1B OP | 1B REG  | 4B SYM
  HVM_OP_SETGLOBAL = 17, // 1B OP | 4B SYM  | 1B REG
  HVM_OP_GETGLOBAL = 18, // 1B OP | 1B REG  | 4B SYM
  
  HVM_GETCLOSURE = 20,   // 1B OP | 1B REG
  
  HVM_ADD = 21,          // 1B OP | 3B REGs
  HVM_SUB = 22,          // 1B OP | 3B REGs
  HVM_MUL = 23,          // 1B OP | 3B REGs
  HVM_DIV = 24,          // 1B OP | 3B REGs
  HVM_MOD = 25,          // 1B OP | 3B REGs
  HVM_POW = 26,          // 1B OP | 3B REGs
  
  HVM_ARRAYPUSH = 27,    // 1B OP | 2B REGS
  HVM_ARRAYSHIFT = 28,   // 1B OP | 2B REGS
  HVM_ARRAYPOP = 29,     // 1B OP | 2B REGS
  HVM_ARRAYUNSHIFT = 30, // 1B OP | 2B REGS
  HVM_ARRAYGET = 31,     // 1B OP | 3B REGS
  HVM_ARRAYSET = 32,     // 1B OP | 3B REGS
  HVM_ARRAYREMOVE = 33,  // 1B OP | 3B REGS
  HVM_ARRAYNEW = 34,     // 1B OP | 2B REGS
  
  HVM_STRUCTSET = 35,    // 1B OP | 3B REGS
  HVM_STRUCTGET = 36,    // 1B OP | 3B REGS
  HVM_STRUCTDELETE = 37, // 1B OP | 3B REGS
  HVM_STRUCTNEW = 38,    // 1B OP | 1B REG
  HVM_STRUCTHAS = 39,    // 1B OP | 3B REGS

} hvm_opcodes;

#endif
