#include <stdlib.h>
#include <stdio.h>

#include "vm.h"
#include "object.h"
#include "chunk.h"
#include "symbol.h"

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

hvm_obj_ref *hvm_chunk_get_constant_object(hvm_vm *vm, hvm_chunk_constant *cnst) {
  hvm_obj_ref* co = cnst->object;
  if(co->type == HVM_STRING) {
    hvm_obj_ref    *ref = hvm_new_obj_ref();
    hvm_obj_string *str = hvm_new_obj_string();
    ref->type  = HVM_STRING;
    ref->flags = ref->flags | HVM_OBJ_FLAG_CONSTANT;
    ref->data.v = str;
    str->data = co->data.v;
    return ref;
  } else if(co->type == HVM_SYMBOL) {
    hvm_obj_ref *ref = hvm_new_obj_ref();
    ref->type = HVM_SYMBOL;
    ref->flags = ref->flags | HVM_OBJ_FLAG_CONSTANT;
    ref->data.u64 = hvm_symbolicate(vm->symbols, co->data.v);
    return ref;
  } else {
    fprintf(stderr, "Can't yet handle object type %s\n", hvm_human_name_for_obj_type(co->type));
    return hvm_const_null;
  }
}

void print_constant_object(hvm_obj_ref* obj) {
  switch(obj->type) {
    case HVM_SYMBOL:
      printf("%s", (char*)obj->data.v);
      break;
    // case HVM_STRING:
    //   printf("%s", (char*)obj->data.v);
    //   break;
    default:
      break;
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
  hvm_chunk_constant **consts = chunk->constants;
  hvm_chunk_constant *cnst;
  printf("consts:\n");
  int i = 0;
  while(*consts != NULL) {
    cnst = *consts;
    printf("  #%-4d 0x%08llX  (%p)  %-9s", i, cnst->index, cnst->object, hvm_human_name_for_obj_type(cnst->object->type));
    print_constant_object(cnst->object);
    printf("\n");
    i++;
    consts++;
  }
  printf("\n");
}
void print_symbols(hvm_chunk *chunk) {
  hvm_chunk_symbol **syms = chunk->symbols;
  hvm_chunk_symbol *sym;
  printf("symbols:\n");
  int i = 0;
  while(*syms != NULL) {
    sym = *syms;
    printf("  0x%08llX  %s\n", sym->index, sym->name);
    i++;
    syms++;
  }
  printf("\n");
}

#define READ_U32(V) *(uint32_t*)(V)
#define READ_U64(V) *(uint64_t*)(V)
#define READ_I32(V) *(int32_t*)(V)
#define READ_I64(V) *(int64_t*)(V)

void hvm_print_data(byte *data, uint64_t size) {
  byte op;
  byte sym, reg1, reg2, reg3, ret;
  uint32_t u32;
  uint64_t u64;
  int64_t  i64;
  unsigned long long int i = 0;

  printf("data:\n");
  while(i < size) {
    op = data[i];
    printf("  0x%08llX  ", i);
    switch(op) {
      case HVM_OP_DIE:// 1B OP
        printf("die\n");
        break;
      case HVM_OP_MOVE:// 1B OP | 1B REG | 1B REG
        reg1 = data[i + 1];
        reg2 = data[i + 2];
        i += 2;
        printf("$%-3d = $%d\n", reg1, reg2);
        break;
      case HVM_OP_SETSTRING:// 1B OP | 1B REG  | 4B CONST
        reg1 = data[i + 1];
        u32 = READ_U32(&data[i + 2]);
        i += 5;
        printf("$%-3d = setstring #%d\n", reg1, u32);
        break;
      case HVM_OP_SETSYMBOL:// 1B OP | 1B REG | 4B CONST
        reg1 = data[i + 1];
        u32  = READ_U32(&data[i + 2]);
        i += 5;
        printf("$%-3d = setsymbol #%d\n", reg1, u32);
        break;
      case HVM_OP_INVOKEPRIMITIVE:// 1B OP | 1B REG | 1B REG
        sym = data[i + 1];
        ret = data[i + 2];
        i += 2;
        printf("$%-3d = invokeprimitive($%d)\n", ret, sym);
        break;
      case HVM_OP_INVOKESYMBOLIC:
        sym = data[i + 1];
        ret = data[i + 2];
        i += 2;
        printf("$%-3d = invokesymbolic($%d)\n", ret, sym);
        break;
      case HVM_OP_LITINTEGER: // 1B OP | 1B REG | 8B LIT
        reg1 = data[i + 1];
        i64  = READ_I64(&data[i + 2]);
        i += 9;
        printf("$%-3d = litinteger(%lld)\n", reg1, i64);
        break;
      case HVM_OP_ADD: // 1B OP | 1B REG | 1B REG | 1B REG
        reg1 = data[i + 1];
        reg2 = data[i + 2];
        reg3 = data[i + 3];
        i += 3;
        printf("$%-3d = $%d + $%d\n", reg1, reg2, reg3);
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
      case HVM_OP_SETGLOBAL: // 1B OP | 1B REG | 1B REG
        reg1 = data[i + 1];
        reg2 = data[i + 2];
        i += 2;
        printf("setglobal[$%d] = $%d\n", reg1, reg2);
        break;
      case HVM_OP_SETLOCAL: // 1B OP | 1B REG | 1B REG
        reg1 = data[i + 1];
        reg2 = data[i + 2];
        i += 2;
        printf("setlocal[$%d] = $%d\n", reg1, reg2);
        break;
      case HVM_OP_GETLOCAL: // 1B OP | 1B REG | 1B REG
        reg1 = data[i + 1];
        reg2 = data[i + 2];
        i += 2;
        printf("$%-3d = getlocal[$%d]\n", reg1, reg2);
        break;

      case HVM_OP_STRUCTGET: // 1B OP | 3B REGS
        reg1 = data[i + 1];
        reg2 = data[i + 2];
        reg3 = data[i + 3];
        i += 3;
        printf("$%-3d = $%d.structget[$%d]\n", reg1, reg2, reg3);
        break;
      case HVM_OP_STRUCTSET: // 1B OP | 3B REGS
        reg1 = data[i + 1];
        reg2 = data[i + 2];
        reg3 = data[i + 3];
        i += 3;
        printf("$%d.structset[$%d] = $%d\n", reg1, reg2, reg3);
        break;
      case HVM_OP_STRUCTNEW: // 1B OP | 1B REG
        reg1 = data[i + 1];
        i += 1;
        printf("$%-3d = structnew\n", reg1);
        break;

      case HVM_OP_ARRAYNEW: // 1B OP | 2B REGS
        reg1 = data[i + 1];
        reg2 = data[i + 2];
        i += 2;
        printf("$%-3d = arraynew[$%d]\n", reg1, reg2);
        break;
      case HVM_OP_ARRAYSET: // 1B OP | 3B REGS
        // A[B] = C
        reg1 = data[i + 1];
        reg2 = data[i + 2];
        reg3 = data[i + 3];
        i += 3;
        printf("$%d.arrayset[$%d] = %d\n", reg1, reg2, reg3);
        break;

      case HVM_OP_GOTOADDRESS: // 1B OP | 1B DEST REG
        reg1 = data[i + 1];
        printf("goto($%d)\n", reg1);
        i += 1;
        break;
      case HVM_OP_GETCLOSURE: // 1B OP | 1B REG
        reg1 = data[i + 1];
        printf("$%-3d = getclosure\n", reg1);
        i += 1;
        break;
      case HVM_OP_LT: // 1B OP | 3B REGs
      case HVM_OP_EQ:
        reg1 = data[i + 1];
        reg2 = data[i + 2];
        reg3 = data[i + 3];
        char *cmp = "?";
        if(op == HVM_OP_LT) {
          cmp = "<";
        } else if(op == HVM_OP_EQ) {
          cmp = "==";
        }
        printf("$%-3d = $%-3d %s $%-3d\n", reg1, reg2, cmp, reg3);
        i += 3;
        break;
      // HVM_OP_GT  = 45,          // 1B OP | 3B REGs
      // HVM_OP_LTE = 46,          // 1B OP | 3B REGs
      // HVM_OP_GTE = 47,          // 1B OP | 3B REGs
      // HVM_OP_EQ  = 48,          // 1B OP | 3B REGs

      case HVM_OP_IF: // 1B OP | 1B REG  | 8B DEST
        reg1 = data[i + 1];
        u64  = READ_U64(&data[i + 2]);
        printf("if($%d, 0x%08llX)\n", reg1, u64);
        i += 9;
        break;
      case HVM_OP_CATCH: // 1B OP | 8B DEST | 1B REG
        u64  = READ_U64(&data[i + 1]);
        reg1 = data[i + 9];
        printf("catch(0x%08llX, $%d)\n", u64, reg1);
        i += 9;
        break;
      case HVM_OP_SETEXCEPTION: // 1B OP | 1B REG
        reg1 = data[i + 1];
        printf("$%-3d = setexception\n", reg1);
        i += 1;
        break;
      case HVM_OP_CLEAREXCEPTION: // 1B OP
        printf("clearexception\n");
        break;
      case HVM_OP_CLEARCATCH: // 1B OP
        printf("clearcatch\n");
        break;
      case HVM_OP_CALLSYMBOLIC:// 1B OP | 3B TAG | 4B CONST | 1B REG
        ret = data[i + 8];
        u32 = READ_U32(&data[i + 4]);
        i += 8;
        printf("$%-3d = callsymbolic #%d\n", ret, u32);
        break;
      default:
        printf("%02X (%d)\n", op, op);
    }
    i++;
  }
  printf("\n");
}

void hvm_chunk_disassemble(hvm_chunk *chunk) {
  print_relocations(chunk);
  print_constants(chunk);
  print_symbols(chunk);
  hvm_print_data(chunk->data, chunk->size);
}
