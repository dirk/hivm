#ifndef HVM_VM_H
#define HVM_VM_H
/// @file vm.h

#include "stdint.h"

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
typedef struct hvm_constant_pool {
  /// Entries in the pool: array of pointers to object references.
  struct hvm_object_ref** entries;
  /// Index of the next index for an entry to be inserted at (ie. the length).
  uint32_t next_index;
  /// Number of entries in the pool.
  uint32_t size;
} hvm_constant_pool;

/// Start with 128 slots in the constant pool
/// @relates hvm_constant_pool
#define HVM_CONSTANT_POOL_INITIAL_SIZE 128
/// @relates hvm_constant_pool
#define HVM_CONSTANT_POOL_GROWTH_RATE  2

#define HVM_GENERAL_REGISTERS 128

/// Instance of the VM.
typedef struct hvm_vm {
  // root stack
  // code
  
  /// Instruction pointer (indexes bytes in the program)
  uint64_t ip;
  /// Data for instructions
  byte* program;
  /// Size of program memory (in bytes)
  uint64_t program_size;
  
  hvm_constant_pool const_pool;
  /// General purpose registers ($r0...$rN)
  struct hvm_obj_ref* general_regs[HVM_GENERAL_REGISTERS];
  // heap
  // object space
} hvm_vm;

/// Create a new virtual machine.
/// @memberof hvm_vm
hvm_vm *hvm_new_vm();
/// Begin executing the virtual machine.
/// @memberof hvm_vm
void hvm_vm_run(hvm_vm*);

/// Opcodes
typedef enum {
  HVM_OP_NOOP = 0,     // 1B OP
  HVM_OP_DIE  = 1,     // 1B OP
  HVM_OP_GOTO = 2,     // 1B OP
  HVM_OP_SETSTRING = 3 // 1B OP | 4B CONST | 1B REG
} hvm_opcodes;

#endif
