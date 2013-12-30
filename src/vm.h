#ifndef HVM_VM_H
#define HVM_VM_H

typedef char hvm_op;
#define HVM_OP_SIZE sizeof(hvm_op)

// Size of chunks to be allocated for storing bytecodes.
#define HVM_GENERATOR_GROW_RATE 65536

typedef struct hvm_generator {
  hvm_op* ops; // Memory for program
  int p_bytes_used; // Number of bytes used in the program
  int p_bytes_remaining; // Bytes remaining
  int ip; // Instruction pointer to next index in ops to write an instruction
} hvm_generator_t;

typedef struct hvm_vm {
  // root stack
  // code
  // instruction pointer
  // heap
  // object space
} hvm_vm_t;

typedef enum hvm_opcode {
  HVM_OP_NOOP = 0,
  HVM_OP_GOTO = 1
} hvm_opcode_t;

#endif
