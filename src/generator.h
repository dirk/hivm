#ifndef HVM_GEN_H
#define HVM_GEN_H

// A-type  = 1B OP | 1B REG  | 1B REG | 1B REG
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
  HVM_GEN_OPD2, // 1B OP | 8B DEST | 1B REG
  HVM_GEN_OPD3, // 1B OP | 1B VAL  | 8B DEST
  HVM_GEN_OPE,  // 1B OP | 4B DIFF
  HVM_GEN_OPF,  // 1B OP
  HVM_GEN_OPG,  // 1B OP | 1B REG | 8B LITERAL
  HVM_GEN_OPH,  // 1B OP | 1B REG | 4B CONST

  // 1B OP | 3B TAG | 8B DEST | 1B REG
  HVM_GEN_OP_CALL,
  HVM_GEN_OP_CALL_LABEL,
  // 1B OP | 3B TAG | 4B CONST | 1B REG
  HVM_GEN_OP_CALLSYMBOLIC_SYMBOL,

  // 1B OP | 3B TAG | 1B REG | 1B REG
  HVM_GEN_OP_INVOKESYMBOLIC,
  HVM_GEN_OP_INVOKEADDRESS,
  // 1B OP | 1B REG | 1B REG (untagged)
  HVM_GEN_OP_INVOKEPRIMITIVE,

  HVM_GEN_OPD1_LABEL, // 1B OP | [8B DEST]
  HVM_GEN_OPD2_LABEL, // 1B OP | [8B DEST] | 1B REG
  HVM_GEN_OPD3_LABEL, // 1B OP | 1B REG    | [8B DEST]
  HVM_GEN_OPH_DATA,   // 1B OP | 1B REG    | [4B CONST]
  HVM_GEN_OPG_LABEL,  // 1B OP | 1B REG    | 8B DEST (i64)
  HVM_GEN_OPB2_SYMBOL,// 1B OP | [4B SYM]  | 1B REG

  HVM_GEN_LABEL,
  HVM_GEN_SUB,
  HVM_GEN_BLOCK,
  HVM_GEN_DEBUG_ENTRY
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
  byte reg;
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
typedef struct hvm_gen_item_op_h {
  HVM_GEN_ITEM_HEAD;
  // 1B OP | 1B REG | 4B CONST
  byte op;
  byte reg;
  uint32_t cnst;
} hvm_gen_item_op_h;

typedef struct hvm_gen_item_op_b2_symbol {
  HVM_GEN_ITEM_HEAD;
  byte op;
  char *symbol;
  byte reg;
} hvm_gen_item_op_b2_symbol;
// Alias
typedef hvm_gen_item_op_b2_symbol hvm_gen_op_callsymbolic_symbol;

typedef struct hvm_gen_item_op_d1_label {
  HVM_GEN_ITEM_HEAD;
  byte op;
  char* dest;
} hvm_gen_item_op_d1_label;

typedef struct hvm_gen_item_op_d2_label {
  HVM_GEN_ITEM_HEAD;
  byte op;
  char *label;
  byte reg;
} hvm_gen_item_op_d2_label;
// Alias
typedef hvm_gen_item_op_d2_label hvm_gen_op_call_label;

typedef struct hvm_gen_item_op_d3_label {
  HVM_GEN_ITEM_HEAD;
  byte op;
  byte reg;
  char *label;
} hvm_gen_item_op_d3_label;

typedef struct hvm_gen_item_op_g_label {
  HVM_GEN_ITEM_HEAD;
  byte op;
  byte reg;
  char* label;
} hvm_gen_item_op_g_label;


typedef struct hvm_gen_op_call {
  HVM_GEN_ITEM_HEAD;
  byte op;
  uint64_t dest;
  byte reg;
} hvm_gen_op_call;

typedef struct hvm_gen_op_invokesymbolic {
  HVM_GEN_ITEM_HEAD;
  byte op;
  byte sym;
  byte ret;
} hvm_gen_op_invokesymbolic;

typedef struct hvm_gen_op_invokeaddress {
  HVM_GEN_ITEM_HEAD;
  byte op;
  byte addr;
  byte ret;
} hvm_gen_op_invokeaddress;

typedef struct hvm_gen_op_invokeprimitive {
  HVM_GEN_ITEM_HEAD;
  byte op;
  byte sym;
  byte ret;
} hvm_gen_op_invokeprimitive;


typedef enum {
  HVM_GEN_DATA_STRING,
  HVM_GEN_DATA_INTEGER,
  HVM_GEN_DATA_SYMBOL
} hvm_gen_data_type;

union hvm_gen_item_data {
  int64_t  i64;
  char*    string;
};

typedef struct hvm_gen_item_op_h_data {
  HVM_GEN_ITEM_HEAD;
  /// Type of the literal data
  hvm_gen_data_type data_type;
  byte op;
  /// Destination register
  byte reg;
  /// Literal data
  union hvm_gen_item_data data;
} hvm_gen_item_op_h_data;

typedef struct hvm_gen_item_debug_entry {
  HVM_GEN_ITEM_HEAD;
  uint64_t      ip;
  uint64_t      line;
  char          *name;
  unsigned char flags;
} hvm_gen_item_debug_entry;

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
///@cond
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
  hvm_gen_item_op_h  op_h;

  hvm_gen_item_op_b2_symbol op_b2_symbol;
  hvm_gen_item_op_d1_label  op_d1_label;
  hvm_gen_item_op_d2_label  op_d2_label;
  hvm_gen_item_op_d3_label  op_d3_label;
  hvm_gen_item_op_g_label   op_g_label;
  hvm_gen_item_op_h_data    op_h_data;

  hvm_gen_op_call op_call;
  hvm_gen_op_call_label op_call_label;
  hvm_gen_op_callsymbolic_symbol op_callsymbolic_symbol;

  hvm_gen_op_invokeprimitive op_invokeprimitive;

  // hvm_gen_item_macro macro;
  hvm_gen_item_label label;
  hvm_gen_item_sub   sub;
  hvm_gen_item_block block;
  hvm_gen_item_debug_entry debug_entry;
///@endcond
} hvm_gen_item;

/// @brief Stores instructions, constants, etc. for a chunk. Can then generate the
///        appropriate bytecode for that chunk.
typedef struct hvm_gen {
  /// Root block of the generator.
  hvm_gen_item_block *block;
  char *file;
} hvm_gen;

hvm_gen *hvm_new_gen();
void hvm_gen_set_file(hvm_gen *gen, char *file);
hvm_gen_item_label *hvm_new_item_label();
hvm_gen_item_block *hvm_new_item_block();


struct hvm_chunk *hvm_gen_chunk(hvm_gen *gen);

void hvm_gen_noop(hvm_gen_item_block *block);
void hvm_gen_die(hvm_gen_item_block *block);
void hvm_gen_jump(hvm_gen_item_block *block, int32_t diff);
void hvm_gen_goto(hvm_gen_item_block *block, uint64_t dest);
void hvm_gen_gotoaddress(hvm_gen_item_block *block, byte reg);
void hvm_gen_call(hvm_gen_item_block *block, uint64_t dest, byte ret);
void hvm_gen_invokesymbolic(hvm_gen_item_block *block, byte sym, byte ret);
void hvm_gen_invokeprimitive(hvm_gen_item_block *block, byte sym, byte ret);
void hvm_gen_if(hvm_gen_item_block *block, byte val, uint64_t dest);
void hvm_gen_return(hvm_gen_item_block *block, byte reg);
void hvm_gen_move(hvm_gen_item_block *block, byte dest, byte src);
void hvm_gen_clearcatch(hvm_gen_item_block *block);
void hvm_gen_clearexception(hvm_gen_item_block *block);
void hvm_gen_setexception(hvm_gen_item_block *block, byte reg);

void hvm_gen_getlocal(hvm_gen_item_block *block, byte val_reg, byte sym_reg);
void hvm_gen_setlocal(hvm_gen_item_block *block, byte sym_reg, byte val_reg);

void hvm_gen_getglobal(hvm_gen_item_block *block, byte val_reg, byte sym_reg);
void hvm_gen_setglobal(hvm_gen_item_block *block, byte sym_reg, byte val_reg);

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

void hvm_gen_setstring(hvm_gen_item_block *block, byte reg, uint32_t cnst);

void hvm_gen_add(hvm_gen_item_block *block, byte a, byte b, byte c);

void hvm_gen_lt(hvm_gen_item_block *block, byte a, byte b, byte c);
void hvm_gen_gt(hvm_gen_item_block *block, byte a, byte b, byte c);
void hvm_gen_lte(hvm_gen_item_block *block, byte a, byte b, byte c);
void hvm_gen_gte(hvm_gen_item_block *block, byte a, byte b, byte c);
void hvm_gen_eq(hvm_gen_item_block *block, byte a, byte b, byte c);

// META-GENERATORS
void hvm_gen_label(hvm_gen_item_block *block, char *name);
void hvm_gen_goto_label(hvm_gen_item_block *block, char *name);
void hvm_gen_set_string(hvm_gen_item_block *block, byte reg, char *string);
void hvm_gen_set_symbol(hvm_gen_item_block *block, byte reg, char *string);
void hvm_gen_set_integer(hvm_gen_item_block *block, byte reg, int64_t integer);
void hvm_gen_push_block(hvm_gen_item_block *block, hvm_gen_item_block *push);

/// Generate a CALL to a subroutine with the label `name` in the chunk. The
/// address is resolved at chunk compile time.
void hvm_gen_call_label(hvm_gen_item_block *block, char *label, byte ret);

/// Generate a CALLSYMBOLIC to a subroutine with a given constant symbol. The
/// symbol name is added to the chunk constant table, which is then
/// symbolicated and adjusted at chunk load time.
void hvm_gen_callsymbolic_symbol(hvm_gen_item_block *block, char *symbol, byte ret);

void hvm_gen_if_label(hvm_gen_item_block *block, byte reg, char *label);
void hvm_gen_catch_label(hvm_gen_item_block *block, char *label, byte reg);

// SYMBOLICATED SUB-ROUTINES
// Call at the head of a sub-routine to set up a symbol in the symbol table
// for the sub-routine.
void hvm_gen_sub(hvm_gen_item_block *block, char *name);

void hvm_gen_litinteger_label(hvm_gen_item_block *block, byte reg, char *label);

// Debug information generators
void hvm_gen_set_debug_line(hvm_gen_item_block *block, uint64_t line);
void hvm_gen_set_debug_entry(hvm_gen_item_block *block, uint64_t line, char *name);

#endif
