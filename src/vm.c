#include <stdlib.h>
#include <stdio.h>

#include "vm.h"
#include "symbol.h"
#include "object.h"
#include "frame.h"

static hvm_obj_ref *hvm_const_null = &(hvm_obj_ref){
  .type = HVM_NULL,
  .data = 0
};

hvm_vm *hvm_new_vm() {
  hvm_vm *vm = malloc(sizeof(hvm_vm));
  vm->ip = 0;
  vm->program_size = HVM_PROGRAM_INITIAL_SIZE;
  vm->program = calloc(sizeof(byte), vm->program_size);
  vm->const_pool.next_index = 0;
  vm->const_pool.size = HVM_CONSTANT_POOL_INITIAL_SIZE;
  vm->const_pool.entries = malloc(sizeof(struct hvm_object_ref*) * 
    vm->const_pool.size);
  return vm;
}

#define READ_U32(V) *(uint32_t*)(V)
#define READ_U64(V) *(uint64_t*)(V)

void hvm_vm_run(hvm_vm *vm) {
  byte instr;
  uint32_t const_index, sym_id;
  unsigned char reg;
  
  for(;;) {
    instr = vm->program[vm->ip];
    switch(instr) {
      case HVM_OP_NOOP:
        fprintf(stderr, "NOOP\n");
        break;
      case HVM_OP_DIE:
        fprintf(stderr, "DIE\n");
        goto end;
      case HVM_OP_SETSTRING:  // 1 = reg, 2-5 = const
      case HVM_OP_SETINTEGER: // 1B OP | 4B CONST | 1B REG
      case HVM_OP_SETFLOAT:
      case HVM_OP_SETSTRUCT:
        // TODO: Type-checking
        reg         = vm->program[vm->ip + 1];
        const_index = READ_U32(&vm->program[vm->ip + 2]);
        fprintf(stderr, "SET: reg(%u) const(%u)\n", reg, const_index);
        vm->general_regs[reg] = hvm_vm_get_const(vm, const_index);
        vm->ip += 5;
        break;
      case HVM_OP_SETNULL: // 1B OP | 1B REG
        reg = vm->program[vm->ip + 1];
        vm->general_regs[reg] = hvm_const_null;
        break;

      case HVM_OP_SETLOCAL: // 1B OP | 4B SYM   | 1B REG
        sym_id = READ_U32(&vm->program[vm->ip + 1]);
        reg    = vm->program[vm->ip + 5];
        fprintf(stderr, "Error: SETLOCAL %u %u not implemented yet\n", sym_id, reg);
        goto end;
      case HVM_OP_GETLOCAL: // 1B OP | 1B REG   | 4B SYM
        reg    = vm->program[vm->ip + 1];
        sym_id = READ_U32(&vm->program[vm->ip + 2]);
        fprintf(stderr, "Error: GETLOCAL %u %u not implemented yet\n", reg, sym_id);
        goto end;
      
      case HVM_OP_SETGLOBAL: // 1B OP | 4B SYM   | 1B REG
        sym_id = READ_U32(&vm->program[vm->ip + 1]);
        reg    = vm->program[vm->ip + 5];
        fprintf(stderr, "Error: SETGLOBAL %u %u not implemented yet\n", sym_id, reg);
        goto end;
      case HVM_OP_GETGLOBAL: // 1B OP | 1B REG   | 4B SYM
        reg    = vm->program[vm->ip + 1];
        sym_id = READ_U32(&vm->program[vm->ip + 2]);
        fprintf(stderr, "Error: GETGLOBAL %u %u not implemented yet\n", reg, sym_id);
        goto end;

      default:
        fprintf(stderr, "Unknown instruction: %u\n", instr);
    }
    vm->ip++;
  }
end:
  return;
  //pass
}

struct hvm_obj_ref* hvm_vm_get_const(hvm_vm *vm, uint32_t id) {
  return hvm_const_pool_get_const(&vm->const_pool, id);
}
void hvm_vm_set_const(hvm_vm *vm, uint32_t id, struct hvm_obj_ref* obj) {
  hvm_const_pool_set_const(&vm->const_pool, id, obj);
}

void hvm_const_pool_expand(hvm_const_pool* pool, uint32_t id) {
  while(id >= pool->size) {
    pool->size = pool->size * HVM_CONSTANT_POOL_GROWTH_RATE;
    pool->entries = realloc(pool->entries, sizeof(struct hvm_obj_ref*) * pool->size);
  }
}

struct hvm_obj_ref* hvm_const_pool_get_const(hvm_const_pool* pool, uint32_t id) {
  // TODO: Out of bounds check
  return pool->entries[id];
}

void hvm_const_pool_set_const(hvm_const_pool* pool, uint32_t id, struct hvm_obj_ref* obj) {
  hvm_const_pool_expand(pool, id);
  pool->entries[id] = obj;
}
