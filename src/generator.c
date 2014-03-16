#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include <glib.h>

#include "vm.h"
#include "chunk.h"
#include "generator.h"

hvm_gen *hvm_new_gen() {
  hvm_gen *gen = malloc(sizeof(hvm_gen));
  gen->block.items = g_array_new(TRUE, TRUE, sizeof(hvm_gen_item));
  return gen;
}

// Internal data used during the generation of a chunk.
struct gen_data {
  // array of positions (hvm_chunk_relocation)
  GArray *relocs;
  // array of constants (hvm_chunk_constant)
  GArray *constants;
  // string symbol name => list of positions
  GHashTable *symbols;
};
void hvm_gen_data_add_symbol(struct gen_data *gd, char *sym, uint32_t idx) {
  GList *positions = g_hash_table_lookup(gd->symbols, sym);
  uint32_t *idxptr = malloc(sizeof(uint32_t));
  *idxptr = idx;
  positions = g_list_append(positions, idxptr);
  g_hash_table_replace(gd->symbols, sym, positions);
}
void hvm_gen_data_add_constant(struct gen_data *gd, hvm_chunk_constant *constant) {
  g_array_append_val(gd->constants, constant);
}
void hvm_gen_data_add_reloc(struct gen_data *gd, hvm_chunk_relocation *reloc) {
  g_array_append_val(gd->relocs, reloc);
}

void hvm_gen_process_block(hvm_chunk *chunk, hvm_gen_item_block *block) {
  // Map labels to indexes
  GHashTable *labels = g_hash_table_new(g_str_hash, g_str_equal);
  // Unmapped labels (built up during processing and then emptied/resolved
  // at the end).
  GList *label_uses = NULL;

  unsigned int len = block->items->len;
  unsigned int i;
  for(i = 0; i < len; i++) {
    // Processing each item
    hvm_gen_item item = g_array_index(block->items, hvm_gen_item, i);
    switch(item.base.type) {
      default:
        fprintf(stderr, "Don't know what to do with item type: %d\n", item.base.type);
        break;
    }
  }
}

struct hvm_chunk *hvm_gen_chunk(hvm_gen *gen) {
  hvm_chunk *chunk = hvm_new_chunk();
  uint64_t start_size = 1024;
  chunk->data = calloc(start_size, sizeof(byte));
  chunk->capacity = start_size;

  struct gen_data gd;
  gd.relocs    = g_array_new(TRUE, TRUE, sizeof(hvm_chunk_relocation*));
  gd.constants = g_array_new(TRUE, TRUE, sizeof(hvm_chunk_constant*));
  gd.symbols   = g_hash_table_new(g_str_hash, g_str_equal);

  hvm_gen_process_block(chunk, &gen->block);

  return chunk;
}

#define GEN_PUSH_ITEM(V) g_array_append_val(block->items, V); 

void hvm_gen_noop(hvm_gen_item_block *block) {
  hvm_gen_item_op_f *noop = malloc(sizeof(hvm_gen_item_op_f));
  noop->type = HVM_GEN_OPF;
  noop->op   = HVM_OP_NOOP;
  GEN_PUSH_ITEM(noop);
}

void hvm_gen_jump(hvm_gen_item_block *block, int32_t diff) {
  hvm_gen_item_op_e *jmp = malloc(sizeof(hvm_gen_item_op_e));
  jmp->type = HVM_GEN_OPE;
  jmp->op = HVM_OP_JUMP;
  jmp->diff = diff;
  GEN_PUSH_ITEM(jmp);
}
void hvm_gen_goto(hvm_gen_item_block *block, uint64_t dest) {
  hvm_gen_item_op_d1 *gt = malloc(sizeof(hvm_gen_item_op_d1));
  gt->type = HVM_GEN_OPD1;
  gt->op = HVM_OP_GOTO;
  gt->dest = dest;
  GEN_PUSH_ITEM(gt);
}
void hvm_gen_call(hvm_gen_item_block *block, uint64_t dest, byte ret) {
  hvm_gen_item_op_d2 *call = malloc(sizeof(hvm_gen_item_op_d2));
  call->type = HVM_GEN_OPD2;
  call->op = HVM_OP_GOTO;
  call->dest = dest;
  call->ret  = ret;
  GEN_PUSH_ITEM(call);
}
void hvm_gen_if(hvm_gen_item_block *block, byte val, uint64_t dest) {
  hvm_gen_item_op_d3 *i = malloc(sizeof(hvm_gen_item_op_d3));
  i->type = HVM_GEN_OPD2;
  i->op = HVM_OP_GOTO;
  i->val  = val;
  i->dest = dest;
  GEN_PUSH_ITEM(i);
}

void hvm_gen_getlocal(hvm_gen_item_block *block, byte reg, uint32_t sym) {
  hvm_gen_item_op_b1 *op = malloc(sizeof(hvm_gen_item_op_b1));
  op->type = HVM_GEN_OPB1;
  op->op   = HVM_OP_GETLOCAL;
  op->reg  = reg;
  op->sym  = sym;
  GEN_PUSH_ITEM(op);
}
void hvm_gen_setlocal(hvm_gen_item_block *block, uint32_t sym, byte reg) {
  hvm_gen_item_op_b2 *op = malloc(sizeof(hvm_gen_item_op_b2));
  op->type = HVM_GEN_OPB2;
  op->op   = HVM_OP_SETLOCAL;
  op->sym  = sym;
  op->reg  = reg;
  GEN_PUSH_ITEM(op);
}

void hvm_gen_getglobal(hvm_gen_item_block *block, byte reg, uint32_t sym) {
  hvm_gen_item_op_b1 *op = malloc(sizeof(hvm_gen_item_op_b1));
  op->type = HVM_GEN_OPB1;
  op->op   = HVM_OP_GETGLOBAL;
  op->reg  = reg;
  op->sym  = sym;
  GEN_PUSH_ITEM(op);
}
void hvm_gen_setglobal(hvm_gen_item_block *block, uint32_t sym, byte reg) {
  hvm_gen_item_op_b2 *op = malloc(sizeof(hvm_gen_item_op_b2));
  op->type = HVM_GEN_OPB2;
  op->op   = HVM_OP_SETGLOBAL;
  op->sym  = sym;
  op->reg  = reg;
  GEN_PUSH_ITEM(op);
}

// 1B OP | 1B REG
void hvm_gen_getclosure(hvm_gen_item_block *block, byte reg) {
  hvm_gen_item_op_a1 *op = malloc(sizeof(hvm_gen_item_op_a1));
  op->type = HVM_GEN_OPA1;
  op->op   = HVM_GETCLOSURE;
  op->reg1 = reg;
  GEN_PUSH_ITEM(op);
}

// 1B OP | 1B REG | 8B LITERAL
void hvm_gen_litinteger(hvm_gen_item_block *block, byte reg, int64_t val) {
  hvm_gen_item_op_g *op = malloc(sizeof(hvm_gen_item_op_g));
  op->type = HVM_GEN_OPG;
  op->op   = HVM_OP_LITINTEGER;
  op->reg  = reg;
  op->lit  = val;
  GEN_PUSH_ITEM(op);
}

// ARRAYS ---------------------------------------------------------------------

// 1B OP | 2B REGS
void hvm_gen_arraypush(hvm_gen_item_block *block, byte arr, byte val) {
  hvm_gen_item_op_a2 *op = malloc(sizeof(hvm_gen_item_op_a2));
  op->type = HVM_GEN_OPA2;
  op->op   = HVM_ARRAYPUSH;
  op->reg1 = arr;
  op->reg2 = val;
  GEN_PUSH_ITEM(op);
}
void hvm_gen_arrayshift(hvm_gen_item_block *block, byte reg, byte arr) {
  hvm_gen_item_op_a2 *op = malloc(sizeof(hvm_gen_item_op_a2));
  op->type = HVM_GEN_OPA2;
  op->op   = HVM_ARRAYSHIFT;
  op->reg1 = reg;
  op->reg2 = arr;
  GEN_PUSH_ITEM(op);
}
void hvm_gen_arraypop(hvm_gen_item_block *block, byte reg, byte arr) {
  hvm_gen_item_op_a2 *op = malloc(sizeof(hvm_gen_item_op_a2));
  op->type = HVM_GEN_OPA2;
  op->op   = HVM_ARRAYPOP;
  op->reg1 = reg;
  op->reg2 = arr;
  GEN_PUSH_ITEM(op);
}
void hvm_gen_arrayunshift(hvm_gen_item_block *block, byte arr, byte val) {
  hvm_gen_item_op_a2 *op = malloc(sizeof(hvm_gen_item_op_a2));
  op->type = HVM_GEN_OPA2;
  op->op   = HVM_ARRAYUNSHIFT;
  op->reg1 = arr;
  op->reg2 = val;
  GEN_PUSH_ITEM(op);
}
// 1B OP | 3B REGS
void hvm_gen_arrayget(hvm_gen_item_block *block, byte reg, byte arr, byte idx) {
  hvm_gen_item_op_a3 *op = malloc(sizeof(hvm_gen_item_op_a3));
  op->type = HVM_GEN_OPA3;
  op->op   = HVM_ARRAYGET;
  op->reg1 = reg;
  op->reg2 = arr;
  op->reg3 = idx;
  GEN_PUSH_ITEM(op);
}
void hvm_gen_arrayremove(hvm_gen_item_block *block, byte reg, byte arr, byte idx) {
  hvm_gen_item_op_a3 *op = malloc(sizeof(hvm_gen_item_op_a3));
  op->type = HVM_GEN_OPA3;
  op->op   = HVM_ARRAYREMOVE;
  op->reg1 = reg;
  op->reg2 = arr;
  op->reg3 = idx;
  GEN_PUSH_ITEM(op);
}
void hvm_gen_arrayset(hvm_gen_item_block *block, byte arr, byte idx, byte val) {
  hvm_gen_item_op_a3 *op = malloc(sizeof(hvm_gen_item_op_a3));
  op->type = HVM_GEN_OPA3;
  op->op   = HVM_ARRAYSET;
  op->reg1 = arr;
  op->reg2 = idx;
  op->reg3 = val;
  GEN_PUSH_ITEM(op);
}
// 1B OP | 2B REGS
void hvm_gen_arraynew(hvm_gen_item_block *block, byte reg, byte size) {
  hvm_gen_item_op_a2 *op = malloc(sizeof(hvm_gen_item_op_a2));
  op->type = HVM_GEN_OPA2;
  op->op   = HVM_ARRAYNEW;
  op->reg1 = reg;
  op->reg2 = size;
  GEN_PUSH_ITEM(op);
}

// STRUCTS --------------------------------------------------------------------

// 1B OP | 3B REGS
void hvm_gen_structget(hvm_gen_item_block *block, byte reg, byte strct, byte key) {
  hvm_gen_item_op_a3 *op = malloc(sizeof(hvm_gen_item_op_a3));
  op->type = HVM_GEN_OPA3;
  op->op   = HVM_STRUCTGET;
  op->reg1 = reg;
  op->reg2 = strct;
  op->reg3 = key;
  GEN_PUSH_ITEM(op);
}
void hvm_gen_structdelete(hvm_gen_item_block *block, byte reg, byte strct, byte key) {
  hvm_gen_item_op_a3 *op = malloc(sizeof(hvm_gen_item_op_a3));
  op->type = HVM_GEN_OPA3;
  op->op   = HVM_STRUCTDELETE;
  op->reg1 = reg;
  op->reg2 = strct;
  op->reg3 = key;
  GEN_PUSH_ITEM(op);
}
void hvm_gen_structset(hvm_gen_item_block *block, byte strct, byte key, byte val) {
  hvm_gen_item_op_a3 *op = malloc(sizeof(hvm_gen_item_op_a3));
  op->type = HVM_GEN_OPA3;
  op->op   = HVM_STRUCTSET;
  op->reg1 = strct;
  op->reg2 = key;
  op->reg3 = val;
  GEN_PUSH_ITEM(op);
}
// 1B OP | 1B REG
void hvm_gen_structnew(hvm_gen_item_block *block, byte reg) {
  hvm_gen_item_op_a1 *op = malloc(sizeof(hvm_gen_item_op_a1));
  op->type = HVM_GEN_OPA2;
  op->op   = HVM_STRUCTNEW;
  op->reg1 = reg;
  GEN_PUSH_ITEM(op);
}
