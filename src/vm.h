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

  hvm_const_pool const_pool;
  /// General purpose registers ($r0...$rN)
  struct hvm_obj_ref* general_regs[HVM_GENERAL_REGISTERS];
  // heap
  // object space
  struct hvm_obj_struct *globals;
} hvm_vm;

/// Create a new virtual machine.
/// @memberof hvm_vm
hvm_vm *hvm_new_vm();
/// Begin executing the virtual machine.
/// @memberof hvm_vm
void hvm_vm_run(hvm_vm*);

void hvm_vm_set_const(hvm_vm*, uint32_t, struct hvm_obj_ref*);
struct hvm_obj_ref* hvm_vm_get_const(hvm_vm*, uint32_t);

struct hvm_obj_ref* hvm_const_pool_get_const(hvm_const_pool*, uint32_t);
void hvm_const_pool_set_const(hvm_const_pool*, uint32_t, struct hvm_obj_ref*);

struct hvm_obj_ref* hvm_get_local(struct hvm_frame*, uint64_t);
void hvm_set_local(struct hvm_frame*, uint64_t, struct hvm_obj_ref*);

struct hvm_obj_ref* hvm_get_global(hvm_vm*, uint64_t);
void hvm_set_global(hvm_vm*, uint64_t, struct hvm_obj_ref*);

/// Opcodes
typedef enum {
  HVM_OP_NOOP = 0,       // 1B OP
  HVM_OP_DIE  = 1,       // 1B OP
  HVM_OP_GOTO = 2,       // 1B OP
  HVM_OP_SETSTRING = 3,  // 1B OP | 4B CONST | 1B REG
  HVM_OP_SETINTEGER = 4, // 1B OP | 4B CONST | 1B REG
  HVM_OP_SETFLOAT = 5,   // 1B OP | 4B CONST | 1B REG
  HVM_OP_SETSTRUCT = 6,  // 1B OP | 4B CONST | 1B REG
  HVM_OP_SETNULL = 7,    // 1B OP | 1B REG

  HVM_OP_SETLOCAL = 10,  // 1B OP | 4B SYM   | 1B REG
  HVM_OP_GETLOCAL = 11,  // 1B OP | 1B REG   | 4B SYM
  HVM_OP_SETGLOBAL = 12, // 1B OP | 4B SYM   | 1B REG
  HVM_OP_GETGLOBAL = 13, // 1B OP | 1B REG   | 4B SYM
  
  HVM_GETCLOSURE = 20,   // 1B OP | 1B REG
  
  HVM_ADD = 21,          // 1B OP | 3B REGs
  HVM_SUB = 22,
  HVM_MUL = 23,
  HVM_DIV = 24,
  HVM_MOD = 25,
  HVM_POW = 26
} hvm_opcodes;

#endif
