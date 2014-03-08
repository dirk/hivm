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

#define HVM_PROGRAM_INITIAL_SIZE 16384

/**
@brief Chunk of instruction code and data (constants, etc.).
*/
typedef struct hvm_chunk {
  
} hvm_chunk;

/**
@brief Stores instructions, constants, etc. for a chunk. Can then generate the
       appropriate bytecode for that chunk.
*/
typedef struct hvm_generator {
  // nothing
} hvm_generator;

/**
Generates bytecode.
@memberof hvm_generator
*/
void hvm_generator_bytecode(hvm_generator*);

/// Constant pool maps an integer to a constant
// Can store approximately 4 billion constants (32-bit indexes)
typedef struct hvm_constant_pool {
  struct hvm_object_ref** entries;
  uint32_t next_index;
  uint32_t size;
} hvm_constant_pool;
// Start with 128 slots in the constant pool
#define HVM_CONSTANT_POOL_INITIAL_SIZE 128
#define HVM_CONSTANT_POOL_GROWTH_RATE  2

#define HVM_GENERAL_REGISTERS 128

/// Instance of the VM.
typedef struct hvm_vm {
  // root stack
  // code
  uint64_t ip; // instruction pointer (indexes bytes in the program)
  
  byte* program; // data for instructions
  uint64_t program_size; // size of program memory (in bytes)
  
  hvm_constant_pool const_pool;
  
  struct hvm_obj_ref* general_regs[HVM_GENERAL_REGISTERS];
  // heap
  // object space
} hvm_vm;

hvm_vm *hvm_new_vm();
void hvm_vm_run(hvm_vm*);

/// Opcodes
typedef enum {
  HVM_OP_NOOP = 0,     // 1B OP
  HVM_OP_DIE  = 1,     // 1B OP
  HVM_OP_GOTO = 2,     // 1B OP
  HVM_OP_SETSTRING = 3 // 1B OP | 4B CONST | 1B REG
} hvm_opcodes;

#endif
