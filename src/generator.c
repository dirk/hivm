#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include <glib.h>

#include "vm.h"
#include "object.h"
#include "chunk.h"
#include "generator.h"

char *gen_strclone(char *str) {
  size_t len = strlen(str);
  char  *clone = malloc(sizeof(char) * (size_t)(len + 1));
  strcpy(clone, str);
  return clone;
}

hvm_gen *hvm_new_gen() {
  hvm_gen *gen = malloc(sizeof(hvm_gen));
  gen->block = hvm_new_item_block();
  gen->file = NULL;
  return gen;
}
void hvm_gen_set_file(hvm_gen *gen, char *file) {
  gen->file = file;
}

hvm_gen_item_block *hvm_new_item_block() {
  hvm_gen_item_block *block = malloc(sizeof(hvm_gen_item_block));
  block->type  = HVM_GEN_BLOCK;
  block->items = g_array_new(TRUE, TRUE, sizeof(hvm_gen_item*));
  return block;
}

/// Internal data used during the generation of a chunk.
struct hvm_gen_data {
  /// Parent generator
  hvm_gen *gen;
  /// Array of positions (hvm_chunk_relocation)
  GArray *relocs;
  /// Array of constants (hvm_chunk_constant)
  GArray *constants;
  /// Array of subroutine symbols (hvm_chunk_symbol)
  GArray *symbols;

  // Used for setting chunk debug entries.
  GArray *debug_entries;
  char *current_name;
  char *current_file;
};
void hvm_gen_data_add_debug_entry(struct hvm_gen_data *gd, uint64_t start, uint64_t end, uint64_t line, char *name, unsigned char flags) {
  hvm_chunk_debug_entry *de = malloc(sizeof(hvm_chunk_debug_entry));
  de->start = start;
  de->end   = end;
  de->line  = line;
  de->name  = name;
  de->flags = flags;
  // if(name == NULL && gd->debug_entries->len > 0) {
  //   uint64_t i = gd->debug_entries->len - 1;
  //   hvm_chunk_debug_entry *prev = g_array_index(gd->debug_entries, hvm_chunk_debug_entry*, i);
  //   de->name = prev->name;
  // }
  de->file  = gd->gen->file;
  g_array_append_val(gd->debug_entries, de);
}
void hvm_gen_data_add_symbol(struct hvm_gen_data *gd, char *sym, uint64_t idx) {
  /*
  GList *positions = g_hash_table_lookup(gd->symbols, sym);
  uint64_t *idxptr = malloc(sizeof(uint64_t));
  *idxptr = idx;
  positions = g_list_append(positions, idxptr);
  g_hash_table_replace(gd->symbols, sym, positions);
  */
  hvm_chunk_symbol *cs = malloc(sizeof(hvm_chunk_symbol));
  cs->index = idx;
  cs->name = gen_strclone(sym);
  g_array_append_val(gd->symbols, cs);
}
uint32_t hvm_gen_data_add_constant(struct hvm_gen_data *gd, hvm_chunk_constant *constant) {
  // TODO: Dedup.
  g_array_append_val(gd->constants, constant);
  return (uint32_t)(gd->constants->len - 1);
}
void hvm_gen_data_add_reloc(struct hvm_gen_data *gd, uint64_t relocation_idx) {
  g_array_append_val(gd->relocs, relocation_idx);
}

// eg. WRITE_OP(a1) becomes:
//   "__attribute(...) void write_op_a1(hvm_chunk, hvm_gen_item_op_a1 *op)"
#define WRITE_OP(NAME) __attribute__((always_inline)) void write_op_##NAME \
                         (hvm_chunk *chunk, struct hvm_gen_data *data, hvm_gen_item_op_##NAME *op)

#define WRITE_TAG() WRITE(1, &zero, byte); \
                    WRITE(2, &zero, byte); \
                    WRITE(3, &zero, byte);

#define WRITE(OFFSET, VAL, SIZE) memcpy(&chunk->data[chunk->size + OFFSET], VAL, sizeof(SIZE));

#define RELOCATION(OFFSET) hvm_gen_data_add_reloc(data, chunk->size + OFFSET);

WRITE_OP(a1) {
  // 1B OP | 1B REG
  WRITE(0, &op->op, byte);
  WRITE(1, &op->reg1, byte);
  chunk->size += 2;
}
WRITE_OP(a2) {
  // 1B OP | 1B REG | 1B REG
  WRITE(0, &op->op, byte);
  WRITE(1, &op->reg1, byte);
  WRITE(2, &op->reg2, byte);
  chunk->size += 3;
}
WRITE_OP(a3) {
  // 1B OP | 1B REG | 1B REG | 1B REG
  WRITE(0, &op->op, byte);
  WRITE(1, &op->reg1, byte);
  WRITE(2, &op->reg2, byte);
  WRITE(3, &op->reg3, byte);
  chunk->size += 4;
}
WRITE_OP(b1) {
  // 1B OP | 1B REG | 4B SYM
  WRITE(0, &op->op, byte);
  WRITE(1, &op->reg, byte);
  WRITE(2, &op->sym, uint32_t);
  chunk->size += 6;
}
WRITE_OP(b2) {
  // 1B OP | 4B SYM | 1B REG
  WRITE(0, &op->op, byte);
  WRITE(1, &op->sym, uint32_t);
  WRITE(5, &op->reg, byte);
  chunk->size += 6;
}
WRITE_OP(d1) {
  // 1B OP | 8B DEST
  WRITE(0, &op->op, byte);
  WRITE(1, &op->dest, uint64_t);
  RELOCATION(1);
  chunk->size += 9;
}
WRITE_OP(d2) {
  // 1B OP | 8B DEST | 1B RET
  WRITE(0, &op->op, byte);
  WRITE(1, &op->dest, uint64_t);
  WRITE(9, &op->reg, byte);
  RELOCATION(1);
  chunk->size += 10;
}
WRITE_OP(d3) {
  // 1B OP | 1B VAL  | 8B DEST
  WRITE(0, &op->op, byte);
  WRITE(1, &op->val, byte);
  WRITE(2, &op->dest, uint64_t);
  RELOCATION(2);
  chunk->size += 10;
}
WRITE_OP(e) {
  // 1B OP | 4B DIFF
  WRITE(0, &op->op, byte);
  WRITE(1, &op->diff, int32_t);
  chunk->size += 5;
}
WRITE_OP(f) {
  // 1B OP
  WRITE(0, &op->op, byte);
  chunk->size += 1;
}
WRITE_OP(g) {
  // 1B OP | 1B REG | 8B LITERAL
  WRITE(0, &op->op, byte);
  WRITE(1, &op->reg, byte);
  WRITE(2, &op->lit, int64_t);
  chunk->size += 10;
}
///@cond
struct label_use {
  char *name;
  uint64_t idx;// Position of the address to be updated.
};
///@endcond

#define GET_LABEL(LABEL, AT) _hvm_gen_get_label(labels, &label_uses, LABEL, AT)

uint64_t _hvm_gen_get_label(GHashTable *labels, GList **label_uses, char *label, uint64_t at) {
  gboolean exists = g_hash_table_lookup_extended(labels, label, NULL, NULL);
  uint64_t dest = 0;
  if(exists) {
    uint64_t *idxptr = g_hash_table_lookup(labels, label);
    dest = *idxptr;
  } else {
    struct label_use *use = malloc(sizeof(struct label_use));
    use->name  = label;
    use->idx   = at;// Add one for the byte for the op.
    *label_uses = g_list_prepend(*label_uses, use);
  }
  return dest;
}

void hvm_gen_process_block(hvm_chunk *chunk, struct hvm_gen_data *data, hvm_gen_item_block *block) {
  // Map labels to indexes
  GHashTable *labels = g_hash_table_new(g_str_hash, g_str_equal);
  // Unmapped labels (built up during processing and then emptied/resolved
  // at the end).
  GList *label_uses = NULL;

  const uint32_t zero = 0;
  uint32_t sub;

  unsigned int len = block->items->len;
  unsigned int i;
  byte op, reg;
  uint64_t *idxptr;
  uint64_t dest;
  int64_t i64;
  gboolean exists;
  hvm_obj_ref *ref;
  hvm_chunk_constant *cnst;
  hvm_gen_item_debug_entry *current_entry = NULL;
  uint64_t start, end, line;
  char *name;
  unsigned char flags;

  for(i = 0; i < len; i++) {
    hvm_chunk_expand_if_necessary(chunk);
    uint64_t idx = chunk->size;// Index into chunk for this instruction
    // Processing each item
    hvm_gen_item *item = g_array_index(block->items, hvm_gen_item*, i);
    switch(item->base.type) {
      case HVM_GEN_DEBUG_ENTRY:
        if(current_entry != NULL) {
          start = current_entry->ip;
          end   = idx - 1;
          line  = current_entry->line;
          name  = current_entry->name;
          flags = current_entry->flags;
          hvm_gen_data_add_debug_entry(data, start, end, line, name, flags);
        }
        current_entry = &item->debug_entry;
        current_entry->ip = idx;
        break;
      case HVM_GEN_BLOCK:
        hvm_gen_process_block(chunk, data, (hvm_gen_item_block*)item);
        break;
      case HVM_GEN_SUB:
        hvm_gen_data_add_symbol(data, item->sub.name, idx);
        // Also add a label for the subroutine
        idxptr = malloc(sizeof(uint64_t));
        *idxptr = idx;
        g_hash_table_replace(labels, item->sub.name, idxptr);
        break;
      case HVM_GEN_LABEL:
        // FIXME: Duplicate labels will leak.
        idxptr = malloc(sizeof(uint64_t));
        *idxptr = idx;
        g_hash_table_replace(labels, item->label.name, idxptr);
        break;
      // case HVM_GEN_BLOCK:
      //   hvm_gen_process_block(chunk, data, &item->block);
      //   break;
      case HVM_GEN_OPA1:
        write_op_a1(chunk, data, &item->op_a1);
        break;
      case HVM_GEN_OPA2:
        write_op_a2(chunk, data, &item->op_a2);
        break;
      case HVM_GEN_OPA3:
        write_op_a3(chunk, data, &item->op_a3);
        break;
      case HVM_GEN_OPB1:
        write_op_b1(chunk, data, &item->op_b1);
        break;
      case HVM_GEN_OPB2:
        write_op_b2(chunk, data, &item->op_b2);
        break;
      case HVM_GEN_OPD1:
        write_op_d1(chunk, data, &item->op_d1);
        break;
      case HVM_GEN_OPD2:
        write_op_d2(chunk, data, &item->op_d2);
        break;
      case HVM_GEN_OPD3:
        write_op_d3(chunk, data, &item->op_d3);
        break;
      case HVM_GEN_OPE:
        write_op_e(chunk, data, &item->op_e);
        break;
      case HVM_GEN_OPF:
        write_op_f(chunk, data, &item->op_f);
        break;
      case HVM_GEN_OPG:
        write_op_g(chunk, data, &item->op_g);
        break;

      case HVM_GEN_OPD1_LABEL:
        op   = item->op_d1_label.op;
        dest = GET_LABEL(item->op_d1_label.dest, chunk->size + 1);
        WRITE(0, &op, byte);
        WRITE(1, &dest, uint64_t);
        RELOCATION(1);
        chunk->size += 9;
        break;
      case HVM_GEN_OPD2_LABEL:
        op   = item->op_d2_label.op;
        dest = GET_LABEL(item->op_d2_label.label, chunk->size + 1);
        reg  = item->op_d2_label.reg;
        WRITE(0, &op, byte);
        WRITE(1, &dest, uint64_t);
        WRITE(9, &reg, byte);
        RELOCATION(1);
        chunk->size += 10;
        break;
      case HVM_GEN_OPD3_LABEL:
        op   = item->op_d3_label.op;
        reg  = item->op_d3_label.reg;
        dest = GET_LABEL(item->op_d3_label.label, chunk->size + 2);
        WRITE(0, &op, byte);
        WRITE(1, &reg, byte);
        WRITE(2, &dest, uint64_t);
        RELOCATION(2);
        chunk->size += 10;
        break;

      // CALLPRIMITIVE and CALLSYMBOLIC have identical binary layouts
      case HVM_GEN_OP_CALLPRIMITIVE:
      case HVM_GEN_OP_CALLSYMBOLIC:
        ref = hvm_new_obj_ref();
        ref->type = HVM_SYMBOL;
        ref->data.v = item->op_callsymbolic.symbol;
        cnst = malloc(sizeof(hvm_chunk_constant));
        // 1 byte for the op, 3 bytes for tag
        cnst->index = chunk->size + 1 + HVM_SUBROUTINE_TAG_SIZE;
        cnst->object = ref;
        sub = hvm_gen_data_add_constant(data, cnst);
        WRITE(0, &item->op_callsymbolic.op, byte);
        WRITE_TAG();
        WRITE(1 + HVM_SUBROUTINE_TAG_SIZE, &sub, uint32_t);
        WRITE(5 + HVM_SUBROUTINE_TAG_SIZE, &item->op_callsymbolic.reg, byte);
        chunk->size += 6 + HVM_SUBROUTINE_TAG_SIZE;
        break;

      case HVM_GEN_OP_INVOKEADDRESS:// 1B OP | 3B TAG | 1B REG | 1B REG
        WRITE(0, &item->op_invokeaddress.op, byte);
        WRITE_TAG();
        WRITE(1 + HVM_SUBROUTINE_TAG_SIZE, &item->op_invokeaddress.addr, byte);
        WRITE(2 + HVM_SUBROUTINE_TAG_SIZE, &item->op_invokeaddress.ret,  byte);
        chunk->size += 3 + HVM_SUBROUTINE_TAG_SIZE;
        break;        

      case HVM_GEN_OP_INVOKEPRIMITIVE:// 1B OP | 1B REG | 1B REG
        WRITE(0, &item->op_invokeprimitive.op, byte);
        WRITE(1, &item->op_invokeprimitive.sym, byte);
        WRITE(2, &item->op_invokeprimitive.ret, byte);
        chunk->size += 3;
        break;

      case HVM_GEN_OPG_LABEL:
        exists = g_hash_table_lookup_extended(labels, item->op_g_label.label, NULL, NULL);
        i64 = 0;
        if(exists) {
          idxptr = g_hash_table_lookup(labels, item->op_g_label.label);
          idx = *idxptr;
          i64 += (int64_t)idx;
        } else {
          struct label_use *use = malloc(sizeof(struct label_use));
          use->name = item->op_g_label.label;
          use->idx  = chunk->size + 2;// Bytes of op and reg
          label_uses = g_list_prepend(label_uses, use);
        }
        WRITE(0, &item->op_g_label.op, byte);
        WRITE(1, &item->op_g_label.reg, byte);
        WRITE(2, &i64, int64_t);
        RELOCATION(2);
        chunk->size += 10;
        break;

      case HVM_GEN_OPH_DATA:
        // Regular data
        ref = hvm_new_obj_ref();
        cnst = malloc(sizeof(hvm_chunk_constant));
        sub = 0;
        cnst->index = chunk->size + 2;// One for op, one for reg.
        if(item->op_h_data.data_type == HVM_GEN_DATA_STRING) {
          ref->type = HVM_STRING;
          // Not bothering with hvm_obj_string since this never actually
          // goes into the VM.
          ref->data.v = item->op_h_data.data.string;
          cnst->object = ref;
          WRITE(0, &item->op_h_data.op, byte);
          WRITE(1, &item->op_h_data.reg, byte);
          sub = hvm_gen_data_add_constant(data, cnst);
        } else if(item->op_h_data.data_type == HVM_GEN_DATA_INTEGER) {
          ref->data.i64 = item->op_h_data.data.i64;
          ref->type = HVM_INTEGER;
          cnst->object = ref;
          WRITE(0, &item->op_h_data.op, byte);
          WRITE(1, &item->op_h_data.reg, byte);
          sub = hvm_gen_data_add_constant(data, cnst);
        } else if(item->op_h_data.data_type == HVM_GEN_DATA_SYMBOL) {
          ref->type = HVM_SYMBOL;
          ref->data.v = item->op_h_data.data.string;
          cnst->object = ref;
          WRITE(0, &item->op_h_data.op, byte);
          WRITE(1, &item->op_h_data.reg, byte);
          sub = hvm_gen_data_add_constant(data, cnst);
        } else {
          WRITE(0, &zero, byte);
          WRITE(1, &zero, byte);
          fprintf(stderr, "Don't know what to do with data type: %d\n", item->op_h_data.data_type);
          // Free cnst so that we don't leak
          free(cnst);
        }
        WRITE(2, &sub, uint32_t);
        chunk->size += 6;
        break;

      case HVM_GEN_OP_CALL:// 1B OP | 3B TAG | 8B DEST | 1B REG
        WRITE(0, &item->op_call.op, byte);
        // Write the tag
        WRITE_TAG();
        WRITE(1 + HVM_SUBROUTINE_TAG_SIZE, &item->op_call.dest, uint64_t);
        WRITE(9 + HVM_SUBROUTINE_TAG_SIZE, &item->op_call.reg, byte);
        RELOCATION(1 + HVM_SUBROUTINE_TAG_SIZE);
        chunk->size += 10 + HVM_SUBROUTINE_TAG_SIZE;
        break;

      case HVM_GEN_OP_CALL_LABEL:// Same as OP_CALL
        {
          uint64_t dest_idx = chunk->size + HVM_SUBROUTINE_TAG_SIZE + 4;
          dest = GET_LABEL(item->op_call_label.label, dest_idx);

          WRITE(0, &item->op_call_label.op, byte);
          WRITE_TAG();
          WRITE(1 + HVM_SUBROUTINE_TAG_SIZE, &dest, uint64_t);
          WRITE(9 + HVM_SUBROUTINE_TAG_SIZE, &item->op_call_label.reg, byte);
          RELOCATION(1 + HVM_SUBROUTINE_TAG_SIZE);
          chunk->size += 10 + HVM_SUBROUTINE_TAG_SIZE;
        }
        break;

      default:
        // Bail out *hard* if we run into something unexpected
        fprintf(stderr, "Don't know what to do with item type: %d\n", item->base.type);
        exit(1);
        return;
    }
  }

  GList *u = g_list_first(label_uses);
  while(u != NULL) {
    struct label_use *use = u->data;
    exists = g_hash_table_lookup_extended(labels, use->name, NULL, NULL);
    if(exists) {
      idxptr = g_hash_table_lookup(labels, use->name);
      uint64_t dest = *idxptr;
      memcpy(&chunk->data[use->idx], &dest, sizeof(uint64_t));
    } else {
      fprintf(stderr, "Label not found: %s\n", use->name);
    }
    free(use);
    u = g_list_next(u);
  }
  g_list_free(label_uses);

  // Close out the final entry
  if(current_entry != NULL) {
    start = current_entry->ip;
    end   = chunk->size - 1;
    line  = current_entry->line;
    name  = current_entry->name;
    flags = current_entry->flags;
    hvm_gen_data_add_debug_entry(data, start, end, line, name, flags);
  }
}

struct hvm_chunk *hvm_gen_chunk(hvm_gen *gen) {
  hvm_chunk *chunk = hvm_new_chunk();
  uint64_t start_size = 1024;
  chunk->data = calloc(start_size, sizeof(byte));
  chunk->capacity = start_size;

  struct hvm_gen_data gd;
  gd.gen       = gen;
  gd.relocs    = g_array_new(TRUE, TRUE, sizeof(uint64_t));
  gd.constants = g_array_new(TRUE, TRUE, sizeof(hvm_chunk_constant*));
  gd.symbols   = g_array_new(TRUE, TRUE, sizeof(hvm_chunk_symbol*));
  gd.debug_entries = g_array_new(TRUE, TRUE, sizeof(hvm_chunk_debug_entry*));

  hvm_gen_process_block(chunk, &gd, gen->block);

  uint64_t i;

  hvm_chunk_relocation **relocs = malloc(sizeof(hvm_chunk_relocation*) * (gd.relocs->len + 1));
  for(i = 0; i < gd.relocs->len; i++) {
    hvm_chunk_relocation *r = malloc(sizeof(hvm_chunk_relocation));
    r->index = g_array_index(gd.relocs, uint64_t, i);
    relocs[i] = r;
  }
  relocs[gd.relocs->len] = NULL;

  hvm_chunk_constant **consts = malloc(sizeof(hvm_chunk_constant*) * (gd.constants->len + 1));
  for(i = 0; i < gd.constants->len; i++) {
    consts[i] = g_array_index(gd.constants, hvm_chunk_constant*, i);
  }
  consts[gd.constants->len] = NULL;

  hvm_chunk_symbol **syms = malloc(sizeof(hvm_chunk_symbol*) * (gd.symbols->len + 1));
  for(i = 0; i < gd.symbols->len; i++) {
    syms[i] = g_array_index(gd.symbols, hvm_chunk_symbol*, i);
  }
  syms[gd.symbols->len] = NULL;

  hvm_chunk_debug_entry **entries = malloc(sizeof(hvm_chunk_debug_entry*) * (gd.debug_entries->len + 1));
  for(i = 0; i < gd.debug_entries->len; i++) {
    hvm_chunk_debug_entry* de = g_array_index(gd.debug_entries, hvm_chunk_debug_entry*, i);
    // fprintf(stderr, "entry:\n");
    // fprintf(stderr, "  start: %llu\n", de->start);
    // fprintf(stderr, "  end: %llu\n", de->end);
    // fprintf(stderr, "  line: %llu\n", de->line);
    // fprintf(stderr, "  name: %s\n", de->name);
    // fprintf(stderr, "  file: %s\n", de->file);
    entries[i] = de;
  }
  entries[gd.debug_entries->len] = NULL;

  chunk->relocs    = relocs;
  chunk->constants = consts;
  chunk->symbols   = syms;
  chunk->debug_entries = entries;

  return chunk;
}

#define GEN_PUSH_ITEM(V) g_array_append_val(block->items, V);

void hvm_gen_noop(hvm_gen_item_block *block) {
  hvm_gen_item_op_f *noop = malloc(sizeof(hvm_gen_item_op_f));
  noop->type = HVM_GEN_OPF;
  noop->op   = HVM_OP_NOOP;
  GEN_PUSH_ITEM(noop);
}
void hvm_gen_die(hvm_gen_item_block *block) {
  hvm_gen_item_op_f *die = malloc(sizeof(hvm_gen_item_op_f));
  die->type = HVM_GEN_OPF;
  die->op   = HVM_OP_DIE;
  GEN_PUSH_ITEM(die);
}
void hvm_gen_clearcatch(hvm_gen_item_block *block) {
  hvm_gen_item_op_f *op = malloc(sizeof(hvm_gen_item_op_f));
  op->type = HVM_GEN_OPF;
  op->op   = HVM_OP_CLEARCATCH;
  GEN_PUSH_ITEM(op);
}
void hvm_gen_clearexception(hvm_gen_item_block *block) {
  hvm_gen_item_op_f *op = malloc(sizeof(hvm_gen_item_op_f));
  op->type = HVM_GEN_OPF;
  op->op   = HVM_OP_CLEAREXCEPTION;
  GEN_PUSH_ITEM(op);
}
void hvm_gen_setexception(hvm_gen_item_block *block, byte reg) {
  hvm_gen_item_op_a1 *op = malloc(sizeof(hvm_gen_item_op_a1));
  op->type = HVM_GEN_OPA1;
  op->op = HVM_OP_SETEXCEPTION;
  op->reg1 = reg;
  GEN_PUSH_ITEM(op);
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
void hvm_gen_gotoaddress(hvm_gen_item_block *block, byte reg) {
  hvm_gen_item_op_a1 *op = malloc(sizeof(hvm_gen_item_op_a1));
  op->type = HVM_GEN_OPA1;
  op->op = HVM_OP_GOTOADDRESS;
  op->reg1 = reg;
  GEN_PUSH_ITEM(op);
}
void hvm_gen_call(hvm_gen_item_block *block, uint64_t dest, byte ret) {
  hvm_gen_item_op_d2 *call = malloc(sizeof(hvm_gen_item_op_d2));
  call->type = HVM_GEN_OP_CALL;
  call->op = HVM_OP_CALL;
  call->dest = dest;
  call->reg  = ret;
  GEN_PUSH_ITEM(call);
}
void hvm_gen_if(hvm_gen_item_block *block, byte val, uint64_t dest) {
  hvm_gen_item_op_d3 *i = malloc(sizeof(hvm_gen_item_op_d3));
  i->type = HVM_GEN_OPD2;
  i->op = HVM_OP_IF;
  i->val  = val;
  i->dest = dest;
  GEN_PUSH_ITEM(i);
}

// 1B OP | 1B REG | 1B REG
void hvm_gen_invokesymbolic(hvm_gen_item_block *block, byte sym, byte ret) {
  hvm_gen_op_invokesymbolic *op = malloc(sizeof(hvm_gen_op_invokesymbolic));
  op->type = HVM_GEN_OP_INVOKESYMBOLIC;
  op->op   = HVM_OP_INVOKESYMBOLIC;
  op->sym = sym;
  op->ret = ret;
  GEN_PUSH_ITEM(op);
}
void hvm_gen_invokeaddress(hvm_gen_item_block *block, byte addr, byte ret) {
  hvm_gen_op_invokeaddress *op = malloc(sizeof(hvm_gen_op_invokeaddress));
  op->type = HVM_GEN_OP_INVOKEADDRESS;
  op->op   = HVM_OP_INVOKEADDRESS;
  op->addr = addr;
  op->ret  = ret;
  GEN_PUSH_ITEM(op);
}
void hvm_gen_invokeprimitive(hvm_gen_item_block *block, byte sym, byte ret) {
  hvm_gen_op_invokeprimitive *op = malloc(sizeof(hvm_gen_op_invokeprimitive));
  op->type = HVM_GEN_OP_INVOKEPRIMITIVE;
  op->op   = HVM_OP_INVOKEPRIMITIVE;
  op->sym  = sym;
  op->ret  = ret;
  GEN_PUSH_ITEM(op);
}

// 1B OP | 1B REG
void hvm_gen_return(hvm_gen_item_block *block, byte reg) {
  hvm_gen_item_op_a1 *op = malloc(sizeof(hvm_gen_item_op_a1));
  op->type = HVM_GEN_OPA1;
  op->op   = HVM_OP_RETURN;
  op->reg1 = reg;
  GEN_PUSH_ITEM(op);
}
// 1B OP | 1B REG | 1B REG
void hvm_gen_move(hvm_gen_item_block *block, byte dest, byte src) {
  hvm_gen_item_op_a2 *op = malloc(sizeof(hvm_gen_item_op_a2));
  op->type = HVM_GEN_OPA2;
  op->op   = HVM_OP_MOVE;
  op->reg1 = dest;
  op->reg2 = src;
  GEN_PUSH_ITEM(op);
}

void hvm_gen_getlocal(hvm_gen_item_block *block, byte val_reg, byte sym_reg) {
  hvm_gen_item_op_a2 *op = malloc(sizeof(hvm_gen_item_op_a2));
  op->type = HVM_GEN_OPA2;
  op->op   = HVM_OP_GETLOCAL;
  op->reg1 = val_reg;
  op->reg2 = sym_reg;
  GEN_PUSH_ITEM(op);
}
void hvm_gen_setlocal(hvm_gen_item_block *block, byte sym_reg, byte val_reg) {
  hvm_gen_item_op_a2 *op = malloc(sizeof(hvm_gen_item_op_a2));
  op->type = HVM_GEN_OPA2;
  op->op   = HVM_OP_SETLOCAL;
  op->reg1 = sym_reg;
  op->reg2  = val_reg;
  GEN_PUSH_ITEM(op);
}

void hvm_gen_getglobal(hvm_gen_item_block *block, byte val_reg, byte sym_reg) {
  hvm_gen_item_op_a2 *op = malloc(sizeof(hvm_gen_item_op_a2));
  op->type = HVM_GEN_OPA2;
  op->op   = HVM_OP_GETGLOBAL;
  op->reg1 = val_reg;
  op->reg2 = sym_reg;
  GEN_PUSH_ITEM(op);
}
void hvm_gen_setglobal(hvm_gen_item_block *block, byte sym_reg, byte val_reg) {
  hvm_gen_item_op_a2 *op = malloc(sizeof(hvm_gen_item_op_a2));
  op->type = HVM_GEN_OPA2;
  op->op   = HVM_OP_SETGLOBAL;
  op->reg1 = sym_reg;
  op->reg2  = val_reg;
  GEN_PUSH_ITEM(op);
}

// 1B OP | 1B REG
void hvm_gen_getclosure(hvm_gen_item_block *block, byte reg) {
  hvm_gen_item_op_a1 *op = malloc(sizeof(hvm_gen_item_op_a1));
  op->type = HVM_GEN_OPA1;
  op->op   = HVM_OP_GETCLOSURE;
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
  op->op   = HVM_OP_ARRAYPUSH;
  op->reg1 = arr;
  op->reg2 = val;
  GEN_PUSH_ITEM(op);
}
void hvm_gen_arrayshift(hvm_gen_item_block *block, byte reg, byte arr) {
  hvm_gen_item_op_a2 *op = malloc(sizeof(hvm_gen_item_op_a2));
  op->type = HVM_GEN_OPA2;
  op->op   = HVM_OP_ARRAYSHIFT;
  op->reg1 = reg;
  op->reg2 = arr;
  GEN_PUSH_ITEM(op);
}
void hvm_gen_arraypop(hvm_gen_item_block *block, byte reg, byte arr) {
  hvm_gen_item_op_a2 *op = malloc(sizeof(hvm_gen_item_op_a2));
  op->type = HVM_GEN_OPA2;
  op->op   = HVM_OP_ARRAYPOP;
  op->reg1 = reg;
  op->reg2 = arr;
  GEN_PUSH_ITEM(op);
}
void hvm_gen_arrayunshift(hvm_gen_item_block *block, byte arr, byte val) {
  hvm_gen_item_op_a2 *op = malloc(sizeof(hvm_gen_item_op_a2));
  op->type = HVM_GEN_OPA2;
  op->op   = HVM_OP_ARRAYUNSHIFT;
  op->reg1 = arr;
  op->reg2 = val;
  GEN_PUSH_ITEM(op);
}
void hvm_gen_arraylen(hvm_gen_item_block *block, byte len, byte arr) {
  hvm_gen_item_op_a2 *op = malloc(sizeof(hvm_gen_item_op_a2));
  op->type = HVM_GEN_OPA2;
  op->op   = HVM_OP_ARRAYLEN;
  op->reg1 = len;
  op->reg2 = arr;
  GEN_PUSH_ITEM(op);
}
// 1B OP | 3B REGS
void hvm_gen_arrayget(hvm_gen_item_block *block, byte reg, byte arr, byte idx) {
  hvm_gen_item_op_a3 *op = malloc(sizeof(hvm_gen_item_op_a3));
  op->type = HVM_GEN_OPA3;
  op->op   = HVM_OP_ARRAYGET;
  op->reg1 = reg;
  op->reg2 = arr;
  op->reg3 = idx;
  GEN_PUSH_ITEM(op);
}
void hvm_gen_arrayremove(hvm_gen_item_block *block, byte reg, byte arr, byte idx) {
  hvm_gen_item_op_a3 *op = malloc(sizeof(hvm_gen_item_op_a3));
  op->type = HVM_GEN_OPA3;
  op->op   = HVM_OP_ARRAYREMOVE;
  op->reg1 = reg;
  op->reg2 = arr;
  op->reg3 = idx;
  GEN_PUSH_ITEM(op);
}
void hvm_gen_arrayset(hvm_gen_item_block *block, byte arr, byte idx, byte val) {
  hvm_gen_item_op_a3 *op = malloc(sizeof(hvm_gen_item_op_a3));
  op->type = HVM_GEN_OPA3;
  op->op   = HVM_OP_ARRAYSET;
  op->reg1 = arr;
  op->reg2 = idx;
  op->reg3 = val;
  GEN_PUSH_ITEM(op);
}
// 1B OP | 2B REGS
void hvm_gen_arraynew(hvm_gen_item_block *block, byte reg, byte size) {
  hvm_gen_item_op_a2 *op = malloc(sizeof(hvm_gen_item_op_a2));
  op->type = HVM_GEN_OPA2;
  op->op   = HVM_OP_ARRAYNEW;
  op->reg1 = reg;
  op->reg2 = size;
  GEN_PUSH_ITEM(op);
}

// STRUCTS --------------------------------------------------------------------

// 1B OP | 3B REGS
void hvm_gen_structget(hvm_gen_item_block *block, byte reg, byte strct, byte key) {
  hvm_gen_item_op_a3 *op = malloc(sizeof(hvm_gen_item_op_a3));
  op->type = HVM_GEN_OPA3;
  op->op   = HVM_OP_STRUCTGET;
  op->reg1 = reg;
  op->reg2 = strct;
  op->reg3 = key;
  GEN_PUSH_ITEM(op);
}
void hvm_gen_structdelete(hvm_gen_item_block *block, byte reg, byte strct, byte key) {
  hvm_gen_item_op_a3 *op = malloc(sizeof(hvm_gen_item_op_a3));
  op->type = HVM_GEN_OPA3;
  op->op   = HVM_OP_STRUCTDELETE;
  op->reg1 = reg;
  op->reg2 = strct;
  op->reg3 = key;
  GEN_PUSH_ITEM(op);
}
void hvm_gen_structset(hvm_gen_item_block *block, byte strct, byte key, byte val) {
  hvm_gen_item_op_a3 *op = malloc(sizeof(hvm_gen_item_op_a3));
  op->type = HVM_GEN_OPA3;
  op->op   = HVM_OP_STRUCTSET;
  op->reg1 = strct;
  op->reg2 = key;
  op->reg3 = val;
  GEN_PUSH_ITEM(op);
}
// 1B OP | 1B REG
void hvm_gen_structnew(hvm_gen_item_block *block, byte reg) {
  hvm_gen_item_op_a1 *op = malloc(sizeof(hvm_gen_item_op_a1));
  op->type = HVM_GEN_OPA1;
  op->op   = HVM_OP_STRUCTNEW;
  op->reg1 = reg;
  GEN_PUSH_ITEM(op);
}

// 1B OP | 1B REG | 4B CONST
void hvm_gen_setstring(hvm_gen_item_block *block, byte reg, uint32_t cnst) {
  hvm_gen_item_op_h *op = malloc(sizeof(hvm_gen_item_op_h));
  op->type = HVM_GEN_OPH;
  op->op   = HVM_OP_SETSTRING;
  op->reg  = reg;
  op->cnst = cnst;
  GEN_PUSH_ITEM(op);
}

// 1B OP | 1B REG | 1B REG | 1B REG
void hvm_gen_add(hvm_gen_item_block *block, byte a, byte b, byte c) {
  hvm_gen_item_op_a3 *op = malloc(sizeof(hvm_gen_item_op_a3));
  op->type = HVM_GEN_OPA3;
  op->op   = HVM_OP_ADD;
  op->reg1 = a;
  op->reg2 = b;
  op->reg3 = c;
  GEN_PUSH_ITEM(op);
}
void hvm_gen_lt(hvm_gen_item_block *block, byte a, byte b, byte c) {
  hvm_gen_item_op_a3 *op = malloc(sizeof(hvm_gen_item_op_a3));
  op->type = HVM_GEN_OPA3;
  op->op   = HVM_OP_LT;
  op->reg1 = a;
  op->reg2 = b;
  op->reg3 = c;
  GEN_PUSH_ITEM(op);
}
void hvm_gen_gt(hvm_gen_item_block *block, byte a, byte b, byte c) {
  hvm_gen_item_op_a3 *op = malloc(sizeof(hvm_gen_item_op_a3));
  op->type = HVM_GEN_OPA3;
  op->op   = HVM_OP_GT;
  op->reg1 = a;
  op->reg2 = b;
  op->reg3 = c;
  GEN_PUSH_ITEM(op);
}
void hvm_gen_lte(hvm_gen_item_block *block, byte a, byte b, byte c) {
  hvm_gen_item_op_a3 *op = malloc(sizeof(hvm_gen_item_op_a3));
  op->type = HVM_GEN_OPA3;
  op->op   = HVM_OP_LTE;
  op->reg1 = a;
  op->reg2 = b;
  op->reg3 = c;
  GEN_PUSH_ITEM(op);
}
void hvm_gen_gte(hvm_gen_item_block *block, byte a, byte b, byte c) {
  hvm_gen_item_op_a3 *op = malloc(sizeof(hvm_gen_item_op_a3));
  op->type = HVM_GEN_OPA3;
  op->op   = HVM_OP_GTE;
  op->reg1 = a;
  op->reg2 = b;
  op->reg3 = c;
  GEN_PUSH_ITEM(op);
}
void hvm_gen_eq(hvm_gen_item_block *block, byte a, byte b, byte c) {
  hvm_gen_item_op_a3 *op = malloc(sizeof(hvm_gen_item_op_a3));
  op->type = HVM_GEN_OPA3;
  op->op   = HVM_OP_EQ;
  op->reg1 = a;
  op->reg2 = b;
  op->reg3 = c;
  GEN_PUSH_ITEM(op);
}
void hvm_gen_and(hvm_gen_item_block *block, byte a, byte b, byte c) {
  hvm_gen_item_op_a3 *op = malloc(sizeof(hvm_gen_item_op_a3));
  op->type = HVM_GEN_OPA3;
  op->op   = HVM_OP_AND;
  op->reg1 = a;
  op->reg2 = b;
  op->reg3 = c;
  GEN_PUSH_ITEM(op);
}


// META-GENERATORS

void hvm_gen_goto_label(hvm_gen_item_block *block, char *name) {
  hvm_gen_item_op_d1_label *gt = malloc(sizeof(hvm_gen_item_op_d1_label));
  gt->type = HVM_GEN_OPD1_LABEL;
  gt->op = HVM_OP_GOTO;
  gt->dest = gen_strclone(name);
  GEN_PUSH_ITEM(gt);
}

void hvm_gen_label(hvm_gen_item_block *block, char *name) {
  hvm_gen_item_label *label = malloc(sizeof(hvm_gen_item_label));
  label->type = HVM_GEN_LABEL;
  label->name = gen_strclone(name);
  GEN_PUSH_ITEM(label);
}

void hvm_gen_set_symbol(hvm_gen_item_block *block, byte reg, char *string) {
  hvm_gen_item_op_h_data *data = malloc(sizeof(hvm_gen_item_op_h_data));
  data->type = HVM_GEN_OPH_DATA;
  data->op = HVM_OP_SETSYMBOL;
  data->reg = reg;
  data->data_type = HVM_GEN_DATA_SYMBOL;
  data->data.string = gen_strclone(string);
  GEN_PUSH_ITEM(data);
}
void hvm_gen_set_string(hvm_gen_item_block *block, byte reg, char *string) {
  hvm_gen_item_op_h_data *data = malloc(sizeof(hvm_gen_item_op_h_data));
  data->type = HVM_GEN_OPH_DATA;
  data->op = HVM_OP_SETSTRING;
  data->reg = reg;
  data->data_type = HVM_GEN_DATA_STRING;
  data->data.string = gen_strclone(string);
  GEN_PUSH_ITEM(data);
}
void hvm_gen_set_integer(hvm_gen_item_block *block, byte reg, int64_t integer) {
  hvm_gen_item_op_h_data *data = malloc(sizeof(hvm_gen_item_op_h_data));
  data->type = HVM_GEN_OPH_DATA;
  data->op = HVM_OP_SETSTRING;
  data->reg = reg;
  data->data_type = HVM_GEN_DATA_INTEGER;
  data->data.i64 = integer;
  GEN_PUSH_ITEM(data);
}

// 1B OP | 1B REG | 8B LITERAL
void hvm_gen_litinteger_label(hvm_gen_item_block *block, byte reg, char *label) {
  hvm_gen_item_op_g_label *data = malloc(sizeof(hvm_gen_item_op_g_label));
  data->type = HVM_GEN_OPG_LABEL;
  data->op = HVM_OP_LITINTEGER;
  data->reg = reg;
  data->label = gen_strclone(label);
  GEN_PUSH_ITEM(data);
}

void hvm_gen_if_label(hvm_gen_item_block *block, byte reg, char *label) {
  hvm_gen_item_op_d3_label *data = malloc(sizeof(hvm_gen_item_op_d3_label));
  data->type = HVM_GEN_OPD3_LABEL;
  data->op = HVM_OP_IF;
  data->reg = reg;
  data->label = gen_strclone(label);
  GEN_PUSH_ITEM(data);
}

void hvm_gen_sub(hvm_gen_item_block *block, char *name) {
  hvm_gen_item_sub *sub = malloc(sizeof(hvm_gen_item_sub));
  sub->type = HVM_GEN_SUB;
  sub->name = gen_strclone(name);
  GEN_PUSH_ITEM(sub);
}
void hvm_gen_call_label(hvm_gen_item_block *block, char *label, byte ret) {
  hvm_gen_op_call_label *call = malloc(sizeof(hvm_gen_op_call_label));
  call->type = HVM_GEN_OP_CALL_LABEL;
  call->op = HVM_OP_CALL;
  call->label = label;
  call->reg  = ret;
  GEN_PUSH_ITEM(call);
}
void hvm_gen_callsymbolic(hvm_gen_item_block *block, char *symbol, byte ret) {
  hvm_gen_op_callsymbolic *call = malloc(sizeof(hvm_gen_op_callsymbolic));
  call->type = HVM_GEN_OP_CALLSYMBOLIC;
  call->op = HVM_OP_CALLSYMBOLIC;
  call->symbol = symbol;
  call->reg = ret;
  GEN_PUSH_ITEM(call);
}
void hvm_gen_callprimitive(hvm_gen_item_block *block, char *symbol, byte ret) {
  hvm_gen_op_callprimitive *call = malloc(sizeof(hvm_gen_op_callprimitive));
  call->type   = HVM_GEN_OP_CALLPRIMITIVE;
  call->op     = HVM_OP_CALLPRIMITIVE;
  call->symbol = symbol;
  call->reg    = ret;
  GEN_PUSH_ITEM(call);
}

void hvm_gen_catch_label(hvm_gen_item_block *block, char *label, byte reg) {
  hvm_gen_item_op_d2_label *catch = malloc(sizeof(hvm_gen_item_op_d2_label));
  catch->type = HVM_GEN_OPD2_LABEL;
  catch->op = HVM_OP_CATCH;
  catch->label = label;
  catch->reg  = reg;
  GEN_PUSH_ITEM(catch);
}

void hvm_gen_push_block(hvm_gen_item_block *block, hvm_gen_item_block *push) {
  GEN_PUSH_ITEM(push);
}

void hvm_gen_set_debug_line(hvm_gen_item_block *block, uint64_t line) {
  hvm_gen_item_debug_entry *prev = NULL;
  for(int i = (int)(block->items->len - 1); i >= 0; i--) {
    hvm_gen_item *item = g_array_index(block->items, hvm_gen_item*, i);
    if(item->base.type == HVM_GEN_DEBUG_ENTRY) {
      prev = &item->debug_entry;
      break;
    }
  }
  hvm_gen_item_debug_entry *entry = malloc(sizeof(hvm_gen_item_debug_entry));
  entry->type = HVM_GEN_DEBUG_ENTRY;
  entry->line = line;
  if(prev != NULL) {
    entry->name  = prev->name;
    entry->flags = prev->flags;
  } else {
    entry->name = NULL;
    entry->flags = 0x0;
  }
  GEN_PUSH_ITEM(entry);
}
void hvm_gen_set_debug_entry(hvm_gen_item_block *block, uint64_t line, char *name) {
  hvm_gen_item_debug_entry *entry = malloc(sizeof(hvm_gen_item_debug_entry));
  entry->type = HVM_GEN_DEBUG_ENTRY;
  entry->line = line;
  entry->name = name;
  entry->flags = 0x0;
  GEN_PUSH_ITEM(entry);
}
void hvm_gen_set_debug_flags(hvm_gen_item_block *block, unsigned char flags) {
  hvm_gen_item_debug_entry *entry = NULL;
  int i;
  for(i = (int)(block->items->len - 1); i >= 0; i--) {
    hvm_gen_item *item = g_array_index(block->items, hvm_gen_item*, i);
    if(item->base.type == HVM_GEN_DEBUG_ENTRY) {
      entry = &item->debug_entry;
      break;
    }
  }
  assert(entry != NULL);
  entry->flags = flags;
}
