#ifndef HVM_VM_H
#define HVM_VM_H
/// @file vm.h

#include "stdint.h"

/// VM opcode (256 max)
typedef char hvm_opcode;
/// VM instruction
typedef uint64_t hvm_instruction;

/// Size of chunks to be allocated for storing bytecodes.
#define HVM_GENERATOR_GROW_RATE 65536

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
typedef struct hvm_constant_pool {
  
} hvm_constant_pool;

/// Instance of the VM.
typedef struct hvm_vm {
  // root stack
  // code
  uint64_t ip; // instruction pointer
  hvm_constant_pool const_pool;
  // heap
  // object space
} hvm_vm;

hvm_vm *hvm_new_vm();

/// Opcodes
typedef enum {
  HVM_OP_NOOP = 0,
  HVM_OP_GOTO = 1
} hvm_opcodes;

#endif
