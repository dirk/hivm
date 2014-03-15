#ifndef HVM_GEN_H
#define HVM_GEN_H

// A-type  = 1B OP | 1B REG  | 1B REG (| 1B REG)
// B1-type = 1B OP | 1B REG  | 4B SYM
// B2-type = 1B OP | 4B SYM  | 1B REG
// C-type  = 1B OP | 1B REG  | 4B CONST
// D-type  = 1B OP | 8B DEST
// E-type  = 1B OP | 4B DIFF
// F-type  = 1B OP

typedef enum {
  HVM_GEN_OPA,
  HVM_GEN_OPB1, // 1B OP | 1B REG | 4B SYM
  HVM_GEN_OPB2, // 1B OP | 4B SYM | 1B REG
  HVM_GEN_OPC,
  HVM_GEN_OPD1,
  HVM_GEN_OPD2,
  HVM_GEN_OPD3,
  HVM_GEN_OPE,
  HVM_GEN_OPF,
  HVM_GEN_MACRO,
  HVM_GEN_LABEL
} hvm_gen_item_type;

// OPCODES --------------------------------------------------------------------
typedef struct hvm_gen_item_op_a {
  hvm_gen_item_type type;
  byte reg1;
  byte reg2;
  byte reg3;
} hvm_gen_item_op_a;
typedef struct hvm_gen_item_op_b1 {
  hvm_gen_item_type type;
  // 1B OP | 1B REG | 4B SYM
  byte op;
  byte reg;
  uint32_t sym;
} hvm_gen_item_op_b1;
typedef struct hvm_gen_item_op_b2 {
  hvm_gen_item_type type;
  // 1B OP | 4B SYM | 1B REG
  byte op;
  uint32_t sym;
  byte reg;
} hvm_gen_item_op_b2;
typedef struct hvm_gen_item_op_c {
  hvm_gen_item_type type;
  byte reg;
  uint32_t cnst;
} hvm_gen_item_op_c;
typedef struct hvm_gen_item_op_d1 {
  hvm_gen_item_type type;
  byte op;
  uint64_t dest;
} hvm_gen_item_op_d1;
typedef struct hvm_gen_item_op_d2 {
  hvm_gen_item_type type;
  byte op;
  uint64_t dest;
  byte ret;
} hvm_gen_item_op_d2;
typedef struct hvm_gen_item_op_d3 {
  hvm_gen_item_type type;
  byte op;
  byte val;
  uint64_t dest;
} hvm_gen_item_op_d3;
typedef struct hvm_gen_item_op_e {
  hvm_gen_item_type type;
  byte op;
  int32_t diff;
} hvm_gen_item_op_e;
typedef struct hvm_gen_item_op_f {
  hvm_gen_item_type type;
  byte op;
} hvm_gen_item_op_f;

/*
typedef enum {
  HVM_MACRO_SUB
} hvm_gen_macro_type;
typedef struct hvm_gen_item_macro {
  hvm_gen_item_type type;
  hvm_gen_macro_type macro;
  char *name;
} hvm_gen_item_macro;
*/

typedef struct hvm_gen_item_label {
  hvm_gen_item_type type;
  char *name;
} hvm_gen_item_label;



typedef union hvm_gen_item {
  hvm_gen_item_op_a  op_a;
  hvm_gen_item_op_a  op_b1;
  hvm_gen_item_op_a  op_b2;
  hvm_gen_item_op_a  op_c;
  hvm_gen_item_op_d1 op_d1;
  hvm_gen_item_op_d2 op_d2;
  hvm_gen_item_op_d3 op_d3;
  hvm_gen_item_op_e  op_e;
  // hvm_gen_item_macro macro;
  hvm_gen_item_label label;
} hvm_gen_item;


/// @brief Stores instructions, constants, etc. for a chunk. Can then generate the
///        appropriate bytecode for that chunk.
typedef struct hvm_gen {
#ifdef GLIB_MAJOR_VERSION
  /// Stream of items (ops and meta-tags).
  GArray *items;
#else
  void *items;
#endif
  // nothing
} hvm_gen;

hvm_gen *hvm_new_gen();
hvm_gen_item_label *hvm_new_item_label();

void hvm_gen_noop(hvm_gen*);
void hvm_gen_jump(hvm_gen*, int32_t);
void hvm_gen_goto(hvm_gen*, uint64_t);
void hvm_gen_call(hvm_gen*, uint64_t, byte);
void hvm_gen_if(hvm_gen*, byte, uint64_t);

void hvm_gen_getlocal(hvm_gen*, byte, uint32_t);
void hvm_gen_setlocal(hvm_gen*, uint32_t, byte);

#endif
