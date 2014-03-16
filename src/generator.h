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
  HVM_GEN_BASE,
  HVM_GEN_OPA1, // 1B OP | 1B REG
  HVM_GEN_OPA2, // 1B OP | 1B REG | 1B REG
  HVM_GEN_OPA3, // 1B OP | 1B REG | 1B REG | 1B REG
  HVM_GEN_OPB1, // 1B OP | 1B REG | 4B SYM
  HVM_GEN_OPB2, // 1B OP | 4B SYM | 1B REG
  HVM_GEN_OPC,
  HVM_GEN_OPD1, // 1B OP | 8B DEST
  HVM_GEN_OPD2, // 1B OP | 8B DEST | 1B RET
  HVM_GEN_OPD3, // 1B OP | 1B VAL  | 8B DEST
  HVM_GEN_OPE,  // 1B OP | 4B DIFF
  HVM_GEN_OPF,  // 1B OP
  HVM_GEN_OPG,  // 1B OP | 1B REG | 8B LITERAL
  HVM_GEN_LABEL,
  HVM_GEN_SUB,
  HVM_GEN_BLOCK
} hvm_gen_item_type;

#define HVM_GEN_ITEM_HEAD hvm_gen_item_type type;

typedef struct hvm_gen_item_base {
  HVM_GEN_ITEM_HEAD;
} hvm_gen_item_base;

// OPCODES --------------------------------------------------------------------
typedef struct hvm_gen_item_op_a1 {
  HVM_GEN_ITEM_HEAD;
  // 1B OP | 1B REG
  byte op;
  byte reg1;
} hvm_gen_item_op_a1;
typedef struct hvm_gen_item_op_a2 {
  HVM_GEN_ITEM_HEAD;
  // 1B OP | 1B REG | 1B REG
  byte op;
  byte reg1;
  byte reg2;
} hvm_gen_item_op_a2;
typedef struct hvm_gen_item_op_a3 {
  HVM_GEN_ITEM_HEAD;
  // 1B OP | 1B REG | 1B REG | 1B REG
  byte op;
  byte reg1;
  byte reg2;
  byte reg3;
} hvm_gen_item_op_a3;
typedef struct hvm_gen_item_op_b1 {
  HVM_GEN_ITEM_HEAD;
  // 1B OP | 1B REG | 4B SYM
  byte op;
  byte reg;
  uint32_t sym;
} hvm_gen_item_op_b1;
typedef struct hvm_gen_item_op_b2 {
  HVM_GEN_ITEM_HEAD;
  // 1B OP | 4B SYM | 1B REG
  byte op;
  uint32_t sym;
  byte reg;
} hvm_gen_item_op_b2;
typedef struct hvm_gen_item_op_c {
  HVM_GEN_ITEM_HEAD;
  byte reg;
  uint32_t cnst;
} hvm_gen_item_op_c;
typedef struct hvm_gen_item_op_d1 {
  HVM_GEN_ITEM_HEAD;
  // 1B OP | 8B DEST
  byte op;
  uint64_t dest;
} hvm_gen_item_op_d1;
typedef struct hvm_gen_item_op_d2 {
  HVM_GEN_ITEM_HEAD;
  byte op;
  uint64_t dest;
  byte ret;
} hvm_gen_item_op_d2;
typedef struct hvm_gen_item_op_d3 {
  HVM_GEN_ITEM_HEAD;
  byte op;
  byte val;
  uint64_t dest;
} hvm_gen_item_op_d3;
typedef struct hvm_gen_item_op_e {
  HVM_GEN_ITEM_HEAD;
  byte op;
  int32_t diff;
} hvm_gen_item_op_e;
typedef struct hvm_gen_item_op_f {
  HVM_GEN_ITEM_HEAD;
  // 1B OP
  byte op;
} hvm_gen_item_op_f;
typedef struct hvm_gen_item_op_g {
  HVM_GEN_ITEM_HEAD;
  // 1B OP | 1B REG | 8B LITERAL
  byte op;
  byte reg;
  int64_t lit;
} hvm_gen_item_op_g;

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

// Labels inside blocks (for use with JUMPs and GOTOs).
typedef struct hvm_gen_item_label {
  HVM_GEN_ITEM_HEAD;
  char *name;
} hvm_gen_item_label;

// Subroutine label
typedef struct hvm_gen_item_sub {
  HVM_GEN_ITEM_HEAD;
  char *name;
} hvm_gen_item_sub;

typedef struct hvm_gen_item_block {
  HVM_GEN_ITEM_HEAD;
#ifdef GLIB_MAJOR_VERSION
  GArray *items;
#else
  void *items;
#endif
} hvm_gen_item_block;

typedef union hvm_gen_item {
  hvm_gen_item_base  base;
  hvm_gen_item_op_a1 op_a1;
  hvm_gen_item_op_a2 op_a2;
  hvm_gen_item_op_a3 op_a3;
  hvm_gen_item_op_b1 op_b1;
  hvm_gen_item_op_b2 op_b2;
  hvm_gen_item_op_c  op_c;
  hvm_gen_item_op_d1 op_d1;
  hvm_gen_item_op_d2 op_d2;
  hvm_gen_item_op_d3 op_d3;
  hvm_gen_item_op_e  op_e;
  hvm_gen_item_op_f  op_f;
  hvm_gen_item_op_g  op_g;
  // hvm_gen_item_macro macro;
  hvm_gen_item_label label;
  hvm_gen_item_sub   sub;
  hvm_gen_item_block block;
} hvm_gen_item;

/// @brief Stores instructions, constants, etc. for a chunk. Can then generate the
///        appropriate bytecode for that chunk.
typedef struct hvm_gen {
  hvm_gen_item_block block;
} hvm_gen;

hvm_gen *hvm_new_gen();
hvm_gen_item_label *hvm_new_item_label();

struct hvm_chunk *hvm_gen_chunk(hvm_gen *gen);

void hvm_gen_noop(hvm_gen_item_block *block);
void hvm_gen_jump(hvm_gen_item_block *block, int32_t diff);
void hvm_gen_goto(hvm_gen_item_block *block, uint64_t dest);
void hvm_gen_call(hvm_gen_item_block *block, uint64_t dest, byte ret);
void hvm_gen_if(hvm_gen_item_block *block, byte val, uint64_t dest);

void hvm_gen_getlocal(hvm_gen_item_block *block, byte reg, uint32_t sym);
void hvm_gen_setlocal(hvm_gen_item_block *block, uint32_t sym, byte reg);

void hvm_gen_getglobal(hvm_gen_item_block *block, byte reg, uint32_t sym);
void hvm_gen_setglobal(hvm_gen_item_block *block, uint32_t sym, byte reg);

void hvm_gen_getclosure(hvm_gen_item_block *block, byte reg);

void hvm_gen_litinteger(hvm_gen_item_block *block, byte reg, int64_t val);

void hvm_gen_arraypush(hvm_gen_item_block *block, byte arr, byte val);
void hvm_gen_arrayshift(hvm_gen_item_block *block, byte reg, byte arr);
void hvm_gen_arraypop(hvm_gen_item_block *block, byte reg, byte arr);
void hvm_gen_arrayunshift(hvm_gen_item_block *block, byte arr, byte val);
void hvm_gen_arrayget(hvm_gen_item_block *block, byte reg, byte arr, byte idx);
void hvm_gen_arrayremove(hvm_gen_item_block *block, byte reg, byte arr, byte idx);
void hvm_gen_arrayset(hvm_gen_item_block *block, byte arr, byte idx, byte val);
void hvm_gen_arraynew(hvm_gen_item_block *block, byte reg, byte size);

void hvm_gen_structget(hvm_gen_item_block *block, byte reg, byte strct, byte key);
void hvm_gen_structdelete(hvm_gen_item_block *block, byte reg, byte strct, byte key);
void hvm_gen_structset(hvm_gen_item_block *block, byte strct, byte key, byte val);
void hvm_gen_structnew(hvm_gen_item_block *block, byte reg);

#endif
