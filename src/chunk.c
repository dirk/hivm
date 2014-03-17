#include <stdlib.h>
#include <stdio.h>

#include "vm.h"
#include "object.h"
#include "chunk.h"

hvm_chunk *hvm_new_chunk() {
  hvm_chunk *chunk = malloc(sizeof(hvm_chunk));
  chunk->data = NULL;
  chunk->size = 0;
  chunk->capacity = 0;
  return chunk;
}

void hvm_chunk_expand(hvm_chunk *chunk) {
  chunk->capacity += 1024;// TODO: Refactor this.
  chunk->data = realloc(chunk->data, sizeof(byte) * chunk->capacity);
}

void hvm_chunk_expand_if_necessary(hvm_chunk *chunk) {
  if((chunk->size + 16) >= chunk->capacity) {
    hvm_chunk_expand(chunk);
  }
}

char *human_name_for_obj_type(hvm_obj_ref* obj) {
  static char *string = "string",
              *unknown = "unknown",
              *symbol = "symbol",
              *integer = "integer";
  switch(obj->type) {
    case HVM_STRING:
      return string;
    case HVM_SYMBOL:
      return symbol;
    case HVM_INTEGER:
      return integer;
    default:
      return unknown;
  }
}

void print_relocations(hvm_chunk *chunk) {
  hvm_chunk_relocation **relocs = chunk->relocs;
  hvm_chunk_relocation *reloc;
  printf("relocations:\n");
  while(*relocs != NULL) {
    reloc = *relocs;
    printf("  0x%08llX\n", reloc->index);
    relocs++;
  }
  printf("\n");
}
void print_constants(hvm_chunk *chunk) {
  hvm_chunk_constant **consts = chunk->consts;
  hvm_chunk_constant *cnst;
  printf("consts:\n");
  int i = 0;
  while(*consts != NULL) {
    cnst = *consts;
    printf("  #%-4d 0x%08llX  (%p)  %s\n", i, cnst->index, cnst->object, human_name_for_obj_type(cnst->object));
    i++;
    consts++;
  }
  printf("\n");
}

#define READ_U32(V) *(uint32_t*)(V)
#define READ_U64(V) *(uint64_t*)(V)
#define READ_I32(V) *(int32_t*)(V)
#define READ_I64(V) *(int64_t*)(V)

void print_data(hvm_chunk *chunk) {
  byte *data = chunk->data;
  byte op;
  byte sym, reg1, ret;
  uint32_t u32;
  uint64_t u64;
  int64_t  i64;
  unsigned long long int i = 0;

  printf("data:\n");
  while(i < chunk->size) {
    op = data[i];
    printf("  0x%08llX  ", i);
    switch(op) {
      case HVM_OP_SETSYMBOL:// 1B OP | 1B REG | 4B CONST
        reg1 = data[i + 1];
        u32  = READ_U32(&data[i + 2]);
        i += 5;
        printf("$%-2d = setsymbol #%d\n", reg1, u32);
        break;
      case HVM_OP_CALLSYMBOLIC:
        sym = data[i + 1];
        ret = data[i + 2];
        i += 2;
        printf("$%-2d = callsymbolic($%d)\n", ret, sym);
        break;
      case HVM_OP_LITINTEGER: // 1B OP | 1B REG | 8B LIT
        reg1 = data[i + 1];
        i64  = READ_I64(&data[i + 2]);
        i += 9;
        printf("$%-2d = litinteger(%lld)\n", ret, i64);
        break;
      case HVM_OP_GOTO: // 1B OP | 8B DEST
        u64 = READ_U64(&data[i + 1]);
        i += 8;
        printf("goto(0x%08llX)\n", u64);
        break;
      case HVM_OP_RETURN: // 1B OP | 1B REG
        reg1 = data[i + 1];
        i += 1;
        printf("return($%d)\n", reg1);
        break;
      default:
        printf("%02X\n", op);
    }
    i++;
  }
  printf("\n");
}

void hvm_chunk_disassemble(hvm_chunk *chunk) {
  print_relocations(chunk);
  print_constants(chunk);
  print_data(chunk);
}
