#include <stdlib.h>
#include <stdio.h>

#include "vm.h"
#include "symbol.h"
#include "object.h"
#include "frame.h"

struct hvm_obj_ref* hvm_const_null = &(hvm_obj_ref){
  .type = HVM_NULL,
  .data = {0}
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
  vm->globals = hvm_new_obj_struct();

  vm->root = hvm_new_frame();
  vm->stack = calloc(HVM_STACK_SIZE, sizeof(struct hvm_frame*));
  vm->stack[0] = vm->root;
  vm->top = vm->root;

  return vm;
}

#define READ_U32(V) *(uint32_t*)(V)
#define READ_U64(V) *(uint64_t*)(V)

#define AREG areg = vm->program[vm->ip + 1];
#define BREG breg = vm->program[vm->ip + 2];
#define CREG creg = vm->program[vm->ip + 3];

void hvm_vm_run(hvm_vm *vm) {
  byte instr;
  uint32_t const_index, sym_id;
  unsigned char reg, areg, breg, creg;
  hvm_obj_ref *a, *b, *c, *arr, *idx, *key, *val, *strct;

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
      case HVM_OP_SETINTEGER: // 1B OP | 4B REG | 4B CONST
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
        hvm_set_local(vm->top, sym_id, vm->general_regs[reg]);
        vm->ip += 5;
        break;
      case HVM_OP_GETLOCAL: // 1B OP | 1B REG   | 4B SYM
        reg    = vm->program[vm->ip + 1];
        sym_id = READ_U32(&vm->program[vm->ip + 2]);
        vm->general_regs[reg] = hvm_get_local(vm->top, sym_id);
        vm->ip += 5;
        break;

      case HVM_OP_SETGLOBAL: // 1B OP | 4B SYM   | 1B REG
        sym_id = READ_U32(&vm->program[vm->ip + 1]);
        reg    = vm->program[vm->ip + 5];
        hvm_set_global(vm, sym_id, vm->general_regs[reg]);
        vm->ip += 5;
        break;
      case HVM_OP_GETGLOBAL: // 1B OP | 1B REG   | 4B SYM
        reg    = vm->program[vm->ip + 1];
        sym_id = READ_U32(&vm->program[vm->ip + 2]);
        vm->general_regs[reg] = hvm_get_global(vm, sym_id);
        vm->ip += 5;
        break;

      case HVM_GETCLOSURE: // 1B OP | 1B REG
        reg = vm->program[vm->ip + 1];
        hvm_obj_ref* ref = hvm_new_obj_ref();
        ref->type = HVM_STRUCTURE;
        ref->data.v = vm->top->locals;
        vm->general_regs[reg] = ref;
        vm->ip += 1;
        break;

      // MATH -----------------------------------------------------------------
      case HVM_ADD:
      case HVM_SUB:
      case HVM_MUL:
      case HVM_DIV:
      case HVM_MOD: // 1B OP | 3B REGs
        // A = B + C
        AREG; BREG; CREG;
        a = vm->general_regs[areg];
        b = vm->general_regs[breg];
        // TODO: Add float support
        if(instr == HVM_ADD)      { c = hvm_obj_int_add(a, b); }
        else if(instr == HVM_SUB) { c = hvm_obj_int_sub(a, b); }
        else if(instr == HVM_MUL) { c = hvm_obj_int_mul(a, b); }
        else if(instr == HVM_DIV) { c = hvm_obj_int_div(a, b); }
        else if(instr == HVM_MOD) { c = hvm_obj_int_mod(a, b); }
        vm->general_regs[creg] = c;
        vm->ip += 3;
        break;

      // ARRAYS ---------------------------------------------------------------
      case HVM_ARRAYPUSH: // 1B OP | 2B REGS
        // A.push(B)
        AREG; BREG;
        a = vm->general_regs[areg];
        b = vm->general_regs[breg];
        hvm_obj_array_push(a, b);
        vm->ip += 2;
        break;
      case HVM_ARRAYUNSHIFT: // 1B OP | 2B REGS
        // A.unshift(B)
        AREG; BREG;
        a = vm->general_regs[areg];
        b = vm->general_regs[breg];
        hvm_obj_array_unshift(a, b);
        vm->ip += 2;
        break;
      case HVM_ARRAYSHIFT: // 1B OP | 2B REGS
        // A = B.shift()
        AREG; BREG;
        b = vm->general_regs[breg];
        vm->general_regs[areg] = hvm_obj_array_shift(b);
        vm->ip += 2;
        break;
      case HVM_ARRAYPOP: // 1B OP | 2B REGS
        // A = B.pop()
        AREG; BREG;
        b = vm->general_regs[breg];
        vm->general_regs[areg] = hvm_obj_array_pop(b);
        vm->ip += 2;
        break;
      case HVM_ARRAYGET: // 1B OP | 3B REGS
        // arrayget V A I -> V = A[I]
        AREG; BREG; CREG;
        arr = vm->general_regs[breg];
        idx = vm->general_regs[creg];
        vm->general_regs[areg] = hvm_obj_array_get(arr, idx);
        vm->ip += 3;
        break;
      case HVM_ARRAYSET: // 1B OP | 3B REGS
        // arrayset A I V -> A[I] = V
        AREG; BREG; CREG;
        arr = vm->general_regs[areg];
        idx = vm->general_regs[breg];
        val = vm->general_regs[creg];
        hvm_obj_array_set(arr, idx, val);
        vm->ip += 3;
        break;
      case HVM_ARRAYREMOVE: // 1B OP | 3B REGS
        // arrayremove V A I
        AREG; BREG; CREG;
        arr = vm->general_regs[breg];
        idx = vm->general_regs[creg];
        vm->general_regs[areg] = hvm_obj_array_remove(arr, idx);
        vm->ip += 3;
        break;
      case HVM_ARRAYNEW: // 1B OP | 2B REGS
        // arraynew A L
        AREG; BREG;
        val = vm->general_regs[breg];
        hvm_obj_array *arr = hvm_new_obj_array_with_length(val);
        a = hvm_new_obj_ref();
        a->type = HVM_ARRAY;
        a->data.v = arr;
        vm->general_regs[areg] = a;
        vm->ip += 2;
        break;

      // STRUCTS --------------------------------------------------------------
      case HVM_STRUCTSET:
        // structset S K V
        AREG; BREG; CREG;
        strct = vm->general_regs[areg];
        key   = vm->general_regs[breg];
        val   = vm->general_regs[creg];
        hvm_obj_struct_set(strct, key, val);
        vm->ip += 3;
        break;
      case HVM_STRUCTGET:
        // structget V S K
        AREG; BREG; CREG;
        strct = vm->general_regs[breg];
        key   = vm->general_regs[creg];
        vm->general_regs[areg] = hvm_obj_struct_get(strct, key);
        vm->ip += 3;
        break;
      case HVM_STRUCTDELETE:
        // structdelete V S K`
        AREG; BREG; CREG;
        strct = vm->general_regs[breg];
        key   = vm->general_regs[creg];
        vm->general_regs[areg] = hvm_obj_struct_delete(strct, key);
        vm->ip += 3;
        break;
      case HVM_STRUCTNEW:
        // structnew S`
        AREG;
        hvm_obj_struct *s = hvm_new_obj_struct();
        strct = hvm_new_obj_ref();
        strct->type = HVM_STRUCTURE;
        strct->data.v = s;
        vm->general_regs[areg] = strct;
        vm->ip += 1;
        break;
      case HVM_STRUCTHAS:
        // structhas B S K
        fprintf(stderr, "STRUCTHAS not implemented yet!\n");
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

hvm_obj_ref* hvm_get_global(hvm_vm *vm, hvm_symbol_id id) {
  hvm_obj_struct* globals = vm->globals;
  return hvm_obj_struct_internal_get(globals, id);
}
void hvm_set_global(hvm_vm* vm, hvm_symbol_id id, struct hvm_obj_ref *global) {
  hvm_obj_struct* globals = vm->globals;
  hvm_obj_struct_internal_set(globals, id, global);
}

void hvm_set_local(struct hvm_frame *frame, hvm_symbol_id id, struct hvm_obj_ref* local) {
  hvm_obj_struct *locals = frame->locals;
  hvm_obj_struct_internal_set(locals, id, local);
}

struct hvm_obj_ref* hvm_get_local(struct hvm_frame *frame, hvm_symbol_id id) {
  hvm_obj_struct *locals = frame->locals;
  hvm_obj_ref    *ref    = hvm_obj_struct_internal_get(locals, id);
  return ref;
}
