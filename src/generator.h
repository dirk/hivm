#ifndef HVM_GEN_H
#define HVM_GEN_H

// A-type  = 1B OP | 1B REG  | 1B REG (| 1B REG)
// B1-type = 1B OP | 1B REG  | 4B SYM
// B2-type = 1B OP | 4B SYM  | 1B REG
// C-type  = 1B OP | 1B REG  | 4B CONST
// D-type  = 1B OP | 8B DEST
// E-type  = 1B OP | 4B DIFF

typedef enum {
  HVM_GEN_OPA,
  HVM_GEN_OPB1,
  HVM_GEN_OPB2,
  HVM_GEN_OPC,
  HVM_GEN_OPD,
  HVM_GEN_OPE,
  HVM_GEN_MACRO,
  HVM_GEN_LABEL
} hvm_gen_item_type;

typedef struct hvm_gen_item_op_a {
  hvm_gen_item_type type;
  byte reg1;
  byte reg2;
  byte reg3;
} hvm_gen_item_op_a;

typedef struct hvm_gen_item_macro {
  hvm_gen_item_type type;
  byte reg;
  uint32_t sym;
} hvm_gen_item_macro;

typedef struct hvm_gen_item_label {
  hvm_gen_item_type type;
  uint32_t sym;
  byte reg;
} hvm_gen_item_label;

union hvm_gen_item {
  hvm_gen_item_op_a  op_a;
  hvm_gen_item_macro macro;
  hvm_gen_item_label label;
};

/// @brief Stores instructions, constants, etc. for a chunk. Can then generate the
///        appropriate bytecode for that chunk.
typedef struct hvm_gen {
  // nothing
} hvm_gen;

#endif
