#ifndef HVM_VM_H
#define HVM_VM_H
/// @file vm.h

/// VM instruction
typedef char hvm_op;
#define HVM_OP_SIZE sizeof(hvm_op)

// Size of chunks to be allocated for storing bytecodes.
#define HVM_GENERATOR_GROW_RATE 65536

/**
Stores instructions, constants, etc. for a chunk. Can then generate the
appropriate bytecode for that chunk.
*/
typedef struct hvm_generator {
  // nothing
} hvm_generator_t;

/**
Generates bytecode.
@memberof hvm_generator
*/
void hvm_generator_generate(hvm_generator_t*);

/// Instance of the VM.
typedef struct hvm_vm {
  // root stack
  // code
  // instruction pointer
  // heap
  // object space
} hvm_vm_t;

/// Valid opcodes.
typedef enum hvm_opcode {
  HVM_OP_NOOP = 0, ///< Does nothing.
  HVM_OP_GOTO = 1
} hvm_opcode_t;

#endif
