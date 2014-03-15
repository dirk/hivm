#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include <glib.h>

#include "vm.h"
#include "generator.h"

hvm_gen *hvm_new_gen() {
  hvm_gen *gen = malloc(sizeof(hvm_gen));
  gen->items = g_array_new(TRUE, TRUE, sizeof(hvm_gen_item));
  return gen;
}

void hvm_gen_noop(hvm_gen *gen) {
  hvm_gen_item_op_f *noop = malloc(sizeof(hvm_gen_item_op_f));
  noop->type = HVM_GEN_OPF;
  noop->op   = HVM_OP_NOOP;
  g_array_append_val(gen->items, noop);
}

void hvm_gen_jump(hvm_gen *gen, int32_t diff) {
  hvm_gen_item_op_e *jmp = malloc(sizeof(hvm_gen_item_op_e));
  jmp->type = HVM_GEN_OPE;
  jmp->op = HVM_OP_JUMP;
  jmp->diff = diff;
  g_array_append_val(gen->items, jmp);
}
void hvm_gen_goto(hvm_gen *gen, uint64_t dest) {
  hvm_gen_item_op_d1 *gt = malloc(sizeof(hvm_gen_item_op_d1));
  gt->type = HVM_GEN_OPD1;
  gt->op = HVM_OP_GOTO;
  gt->dest = dest;
  g_array_append_val(gen->items, gt);
}
void hvm_gen_call(hvm_gen *gen, uint64_t dest, byte ret) {
  hvm_gen_item_op_d2 *call = malloc(sizeof(hvm_gen_item_op_d2));
  call->type = HVM_GEN_OPD2;
  call->op = HVM_OP_GOTO;
  call->dest = dest;
  call->ret  = ret;
  g_array_append_val(gen->items, call);
}
void hvm_gen_if(hvm_gen *gen, byte val, uint64_t dest) {
  hvm_gen_item_op_d3 *i = malloc(sizeof(hvm_gen_item_op_d3));
  i->type = HVM_GEN_OPD2;
  i->op = HVM_OP_GOTO;
  i->val  = val;
  i->dest = dest;
  g_array_append_val(gen->items, i);
}

void hvm_gen_getlocal(hvm_gen *gen, byte reg, uint32_t sym) {
  hvm_gen_item_op_b1 *op = malloc(sizeof(hvm_gen_item_op_b1));
  op->type = HVM_GEN_OPB1;
  op->op   = HVM_OP_GETLOCAL;
  op->reg  = reg;
  op->sym  = sym;
  g_array_append_val(gen->items, op);
}
void hvm_gen_setlocal(hvm_gen *gen, uint32_t sym, byte reg) {
  hvm_gen_item_op_b2 *op = malloc(sizeof(hvm_gen_item_op_b2));
  op->type = HVM_GEN_OPB2;
  op->op   = HVM_OP_SETLOCAL;
  op->sym  = sym;
  op->reg  = reg;
  g_array_append_val(gen->items, op);
}

void hvm_gen_getglobal(hvm_gen *gen, byte reg, uint32_t sym) {
  hvm_gen_item_op_b1 *op = malloc(sizeof(hvm_gen_item_op_b1));
  op->type = HVM_GEN_OPB1;
  op->op   = HVM_OP_GETGLOBAL;
  op->reg  = reg;
  op->sym  = sym;
  g_array_append_val(gen->items, op);
}
void hvm_gen_setglobal(hvm_gen *gen, uint32_t sym, byte reg) {
  hvm_gen_item_op_b2 *op = malloc(sizeof(hvm_gen_item_op_b2));
  op->type = HVM_GEN_OPB2;
  op->op   = HVM_OP_SETGLOBAL;
  op->sym  = sym;
  op->reg  = reg;
  g_array_append_val(gen->items, op);
}

// 1B OP | 1B REG
void hvm_gen_getclosure(hvm_gen *gen, byte reg) {
  hvm_gen_item_op_a1 *op = malloc(sizeof(hvm_gen_item_op_a1));
  op->type = HVM_GEN_OPA1;
  op->op   = HVM_GETCLOSURE;
  op->reg1 = reg;
  g_array_append_val(gen->items, op);
}

// 1B OP | 1B REG | 8B LITERAL
void hvm_gen_litinteger(hvm_gen *gen, byte reg, int64_t val) {
  hvm_gen_item_op_g *op = malloc(sizeof(hvm_gen_item_op_g));
  op->type = HVM_GEN_OPG;
  op->op   = HVM_OP_LITINTEGER;
  op->reg  = reg;
  op->lit  = val;
  g_array_append_val(gen->items, op);
}

// ARRAYS ---------------------------------------------------------------------

// 1B OP | 2B REGS
void hvm_gen_arraypush(hvm_gen *gen, byte arr, byte val) {
  hvm_gen_item_op_a2 *op = malloc(sizeof(hvm_gen_item_op_a2));
  op->type = HVM_GEN_OPA2;
  op->op   = HVM_ARRAYPUSH;
  op->reg1 = arr;
  op->reg2 = val;
  g_array_append_val(gen->items, op);
}
void hvm_gen_arrayshift(hvm_gen *gen, byte reg, byte arr) {
  hvm_gen_item_op_a2 *op = malloc(sizeof(hvm_gen_item_op_a2));
  op->type = HVM_GEN_OPA2;
  op->op   = HVM_ARRAYSHIFT;
  op->reg1 = reg;
  op->reg2 = arr;
  g_array_append_val(gen->items, op);
}
void hvm_gen_arraypop(hvm_gen *gen, byte reg, byte arr) {
  hvm_gen_item_op_a2 *op = malloc(sizeof(hvm_gen_item_op_a2));
  op->type = HVM_GEN_OPA2;
  op->op   = HVM_ARRAYPOP;
  op->reg1 = reg;
  op->reg2 = arr;
  g_array_append_val(gen->items, op);
}
void hvm_gen_arrayunshift(hvm_gen *gen, byte arr, byte val) {
  hvm_gen_item_op_a2 *op = malloc(sizeof(hvm_gen_item_op_a2));
  op->type = HVM_GEN_OPA2;
  op->op   = HVM_ARRAYUNSHIFT;
  op->reg1 = arr;
  op->reg2 = val;
  g_array_append_val(gen->items, op);
}
// 1B OP | 3B REGS
void hvm_gen_arrayget(hvm_gen *gen, byte reg, byte arr, byte idx) {
  hvm_gen_item_op_a3 *op = malloc(sizeof(hvm_gen_item_op_a3));
  op->type = HVM_GEN_OPA3;
  op->op   = HVM_ARRAYGET;
  op->reg1 = reg;
  op->reg2 = arr;
  op->reg3 = idx;
  g_array_append_val(gen->items, op);
}
void hvm_gen_arrayremove(hvm_gen *gen, byte reg, byte arr, byte idx) {
  hvm_gen_item_op_a3 *op = malloc(sizeof(hvm_gen_item_op_a3));
  op->type = HVM_GEN_OPA3;
  op->op   = HVM_ARRAYREMOVE;
  op->reg1 = reg;
  op->reg2 = arr;
  op->reg3 = idx;
  g_array_append_val(gen->items, op);
}
void hvm_gen_arrayset(hvm_gen *gen, byte arr, byte idx, byte val) {
  hvm_gen_item_op_a3 *op = malloc(sizeof(hvm_gen_item_op_a3));
  op->type = HVM_GEN_OPA3;
  op->op   = HVM_ARRAYSET;
  op->reg1 = arr;
  op->reg2 = idx;
  op->reg3 = val;
  g_array_append_val(gen->items, op);
}
// 1B OP | 2B REGS
void hvm_gen_arraynew(hvm_gen *gen, byte reg, byte size) {
  hvm_gen_item_op_a2 *op = malloc(sizeof(hvm_gen_item_op_a2));
  op->type = HVM_GEN_OPA2;
  op->op   = HVM_ARRAYNEW;
  op->reg1 = reg;
  op->reg2 = size;
  g_array_append_val(gen->items, op);
}

// STRUCTS --------------------------------------------------------------------

// 1B OP | 3B REGS
void hvm_gen_structget(hvm_gen *gen, byte reg, byte strct, byte key) {
  hvm_gen_item_op_a3 *op = malloc(sizeof(hvm_gen_item_op_a3));
  op->type = HVM_GEN_OPA3;
  op->op   = HVM_STRUCTGET;
  op->reg1 = reg;
  op->reg2 = strct;
  op->reg3 = key;
  g_array_append_val(gen->items, op);
}
void hvm_gen_structdelete(hvm_gen *gen, byte reg, byte strct, byte key) {
  hvm_gen_item_op_a3 *op = malloc(sizeof(hvm_gen_item_op_a3));
  op->type = HVM_GEN_OPA3;
  op->op   = HVM_STRUCTDELETE;
  op->reg1 = reg;
  op->reg2 = strct;
  op->reg3 = key;
  g_array_append_val(gen->items, op);
}
void hvm_gen_structset(hvm_gen *gen, byte strct, byte key, byte val) {
  hvm_gen_item_op_a3 *op = malloc(sizeof(hvm_gen_item_op_a3));
  op->type = HVM_GEN_OPA3;
  op->op   = HVM_STRUCTSET;
  op->reg1 = strct;
  op->reg2 = key;
  op->reg3 = val;
  g_array_append_val(gen->items, op);
}
// 1B OP | 1B REG
void hvm_gen_structnew(hvm_gen *gen, byte reg) {
  hvm_gen_item_op_a1 *op = malloc(sizeof(hvm_gen_item_op_a1));
  op->type = HVM_GEN_OPA2;
  op->op   = HVM_STRUCTNEW;
  op->reg1 = reg;
  g_array_append_val(gen->items, op);
}
