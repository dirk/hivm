#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "vm.h"
#include "symbol.h"
#include "object.h"
#include "frame.h"
#include "chunk.h"
// #include "generator.h"

struct hvm_obj_ref* hvm_const_null = &(hvm_obj_ref){
  .type = HVM_NULL,
  .data = {0}
};

hvm_vm *hvm_new_vm() {
  hvm_vm *vm = malloc(sizeof(hvm_vm));
  vm->ip = 0;
  vm->program_capacity = HVM_PROGRAM_INITIAL_CAPACITY;
  vm->program_size = 0;
  vm->program = calloc(sizeof(byte), vm->program_capacity);
  vm->const_pool.next_index = 0;
  vm->const_pool.size = HVM_CONSTANT_POOL_INITIAL_SIZE;
  vm->const_pool.entries = malloc(sizeof(struct hvm_object_ref*) * 
    vm->const_pool.size);
  vm->globals = hvm_new_obj_struct();
  vm->symbol_table = hvm_new_obj_struct();
  vm->symbols = hvm_new_symbol_store();

  vm->stack = calloc(HVM_STACK_SIZE, sizeof(struct hvm_frame*));
  vm->stack_depth = 0;
  vm->root = hvm_new_frame();
  vm->stack[0] = vm->root;
  vm->top = vm->stack[0];

  return vm;
}

void hvm_vm_expand_program(hvm_vm *vm) {
  vm->program_capacity = HVM_PROGRAM_GROW_FUNCTION(vm->program_capacity);
  vm->program = realloc(vm->program, sizeof(byte) * vm->program_capacity);
}

void hvm_vm_load_chunk_symbols(hvm_vm *vm, uint64_t start, hvm_chunk_symbol **syms) {
  hvm_chunk_symbol *sym;
  while(*syms != NULL) {
    sym = *syms;
    uint64_t dest   = start + sym->index;
    uint64_t sym_id = hvm_symbolicate(vm->symbols, sym->name);
    hvm_obj_ref *entry = malloc(sizeof(hvm_obj_ref));
    entry->type = HVM_INTERNAL;
    entry->data.u64 = dest;
    hvm_obj_struct_internal_set(vm->symbol_table, sym_id, entry);

    syms++;
  }
}
void hvm_vm_load_chunk_constants(hvm_vm *vm, uint64_t start, hvm_chunk_constant **consts) {
  hvm_chunk_constant *cnst;
  while(*consts != NULL) {
    cnst = *consts;
    hvm_obj_ref *obj = hvm_chunk_get_constant_object(vm, cnst);
    uint32_t const_id = hvm_vm_add_const(vm, obj);
    memcpy(&vm->program[start + cnst->index], &const_id, sizeof(uint32_t));

    consts++;
  }
  printf("\n");
}
void hvm_vm_load_chunk_relocations(hvm_vm *vm, uint64_t start, hvm_chunk_relocation **relocs) {
  hvm_chunk_relocation *reloc;
  while(*relocs != NULL) {
    reloc = *relocs;
    uint64_t index = reloc->index;
    uint64_t dest;
    // Get the dest.
    memcpy(&dest, &vm->program[start + index], sizeof(uint64_t));
    // Update the dest.
    dest += start;
    // Then write the dest back.
    memcpy(&vm->program[start + index], &dest, sizeof(uint64_t));
    relocs++;
  }
}

void hvm_vm_load_chunk(hvm_vm *vm, void *cv) {
  hvm_chunk *chunk = cv;
  while((vm->program_size + chunk->size + 16) > vm->program_capacity) {
    hvm_vm_expand_program(vm);
  }
  uint64_t start = vm->program_size;
  // Copy over the main chunk data.
  memcpy(&vm->program[start], chunk->data, sizeof(byte) * chunk->size);
  vm->program_size += chunk->size;
  // Copy over the stuff from the chunk header.
  hvm_vm_load_chunk_symbols(vm, start, chunk->symbols);
  hvm_vm_load_chunk_constants(vm, start, chunk->constants);
  hvm_vm_load_chunk_relocations(vm, start, chunk->relocs);
}

#define READ_U32(V) *(uint32_t*)(V)
#define READ_U64(V) *(uint64_t*)(V)
#define READ_I32(V) *(int32_t*)(V)
#define READ_I64(V) *(int64_t*)(V)

#define AREG areg = vm->program[vm->ip + 1];
#define BREG breg = vm->program[vm->ip + 2];
#define CREG creg = vm->program[vm->ip + 3];

void hvm_vm_run(hvm_vm *vm) {
  byte instr;
  uint32_t const_index;
  uint64_t dest, sym_id;//, return_addr;
  int32_t diff;
  int64_t i64_literal;
  unsigned char reg, areg, breg, creg;
  hvm_obj_ref *a, *b, *c, *arr, *idx, *key, *val, *strct;
  hvm_frame *frame, *parent_frame;

  for(; vm->ip < vm->program_size;) {
    instr = vm->program[vm->ip];
    switch(instr) {
      case HVM_OP_NOOP:
        fprintf(stderr, "NOOP\n");
        break;
      case HVM_OP_DIE:
        fprintf(stderr, "DIE\n");
        goto end;
      case HVM_OP_TAILCALL: // 1B OP | 8B DEST
        dest = READ_U64(&vm->program[vm->ip + 1]);
        parent_frame = vm->top;
        frame        = hvm_new_frame();
        frame->return_addr     = parent_frame->return_addr;
        frame->return_register = parent_frame->return_register;
        vm->ip = dest;
        vm->top = (vm->stack[vm->stack_depth] = frame);
        continue;
      case HVM_OP_CALL: // 1B OP | 8B DEST | 1B REG
        dest = READ_U64(&vm->program[vm->ip + 1]);
        reg  = vm->program[vm->ip + 9];
        frame = hvm_new_frame();
        frame->return_addr     = vm->ip + 10; // Instruction is 10 bytes long.
        frame->return_register = reg;
        vm->ip = dest;
        vm->stack_depth += 1;
        vm->top = (vm->stack[vm->stack_depth] = frame);
        continue;
      case HVM_OP_CALLSYMBOLIC:// 1B OP | 1B REG | 1B REG
        AREG; BREG;
        key = vm->general_regs[areg];// This is the symbol we need to look up.
        assert(key->type == HVM_SYMBOL);
        sym_id = key->data.u64;
        val    = hvm_obj_struct_internal_get(vm->symbol_table, sym_id);
        dest   = val->data.u64;
        assert(val->type == HVM_INTERNAL);
        fprintf(stderr, "CALLSYMBOLIC(0x%08llX, $%d)\n", dest, breg);
        frame = hvm_new_frame();
        frame->return_addr = vm->ip + 3;// Instruction is 3 bytes long
        frame->return_register = breg;
        vm->ip = dest;
        vm->stack_depth += 1;
        vm->top = (vm->stack[vm->stack_depth] = frame);
        continue;

      case HVM_OP_CALLADDRESS: // 1B OP | 1B REG | 1B REG
        reg  = vm->program[vm->ip + 1];
        val  = vm->general_regs[reg];
        assert(val->type == HVM_INTEGER);
        dest = (uint64_t)val->data.i64;
        reg  = vm->program[vm->ip + 2]; // Return register now
        frame = hvm_new_frame();
        frame->return_addr     = vm->ip + 3; // Instruction 3 bytes long.
        frame->return_register = reg;
        vm->ip = dest;
        vm->stack_depth += 1;
        vm->top = (vm->stack[vm->stack_depth] = frame);
        continue;
      case HVM_OP_RETURN: // 1B OP | 1B REG
        reg = vm->program[vm->ip + 1];
        // Current frame
        frame = vm->top;
        vm->ip = frame->return_addr;
        vm->stack_depth -= 1;
        vm->top = vm->stack[vm->stack_depth];
        vm->general_regs[frame->return_register] = vm->general_regs[reg];
        fprintf(stderr, "RETURN(0x%08llX) $%d -> $%d\n", frame->return_addr, reg, frame->return_register);
        continue;
      case HVM_OP_JUMP: // 1B OP | 4B DIFF
        diff = READ_I32(&vm->program[vm->ip + 1]);
        if(diff >= 0) {
          vm->ip += (uint64_t)diff;
        } else {
          // TODO: Check for reverse-overflow (ie. abs(diff) > vm->ip)
          vm->ip -= (uint64_t)(diff * -1);
        }
        continue;
      case HVM_OP_GOTO: // 1B OP | 8B DEST
        dest = READ_U64(&vm->program[vm->ip + 1]);
        vm->ip = dest;
        continue;
      case HVM_OP_IF: // 1B OP | 1B REG  | 8B DEST
        reg  = vm->program[vm->ip + 1];
        dest = READ_U64(&vm->program[vm->ip + 2]);
        val  = vm->general_regs[reg];
        if(val->type == HVM_NULL || (val->type == HVM_INTEGER && val->data.i64 == 0)) {
          // Falsey, add on the 9 bytes for the instruction parameters and continue onwards.
          vm->ip += 9;
          break;
        } else {
          // Truthy, go straight to destination.
          vm->ip = dest;
          continue;
        }

      case HVM_OP_LITINTEGER: // 1B OP | 1B REG | 8B LIT
        reg         = vm->program[vm->ip + 1];
        i64_literal = READ_I64(&vm->program[vm->ip + 2]);
        val         = hvm_new_obj_int();
        val->data.i64 = i64_literal;
        vm->general_regs[reg] = val;
        vm->ip += 9;
        break;
      
      case HVM_OP_SETSTRING:  // 1 = reg, 2-5 = const
      case HVM_OP_SETINTEGER: // 1B OP | 1B REG | 4B CONST
      case HVM_OP_SETFLOAT:
      case HVM_OP_SETSTRUCT:
      case HVM_OP_SETSYMBOL:
        // TODO: Type-checking
        reg         = vm->program[vm->ip + 1];
        const_index = READ_U32(&vm->program[vm->ip + 2]);
        fprintf(stderr, "SET $%u = const(%u)\n", reg, const_index);
        vm->general_regs[reg] = hvm_vm_get_const(vm, const_index);
        vm->ip += 5;
        break;
      case HVM_OP_SETNULL: // 1B OP | 1B REG
        reg = vm->program[vm->ip + 1];
        vm->general_regs[reg] = hvm_const_null;
        vm->ip += 1;
        break;

      // case HVM_OP_SETSYMBOL: // 1B OP | 1B REG | 4B CONST
      //   reg = vm->program[vm->ip + 1];
      //   const_index = READ_U32(&vm->program[vm->ip + 2]);
      //   vm->general_regs[reg] = hvm_vm_get_const(vm, const_index);

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
      case HVM_OP_ADD:
      case HVM_OP_SUB:
      case HVM_OP_MUL:
      case HVM_OP_DIV:
      case HVM_OP_MOD: // 1B OP | 3B REGs
        // A = B + C
        AREG; BREG; CREG;
        b = vm->general_regs[breg];
        c = vm->general_regs[creg];
        // TODO: Add float support
        if(instr == HVM_OP_ADD)      { a = hvm_obj_int_add(b, c); }
        else if(instr == HVM_OP_SUB) { a = hvm_obj_int_sub(b, c); }
        else if(instr == HVM_OP_MUL) { a = hvm_obj_int_mul(b, c); }
        else if(instr == HVM_OP_DIV) { a = hvm_obj_int_div(b, c); }
        else if(instr == HVM_OP_MOD) { a = hvm_obj_int_mod(b, c); }
        vm->general_regs[areg] = a;
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

      // TODO: Implement SYMBOLICATE.

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
uint32_t hvm_vm_add_const(hvm_vm *vm, struct hvm_obj_ref* obj) {
  uint32_t id = vm->const_pool.next_index;
  hvm_vm_set_const(vm, id, obj);
  vm->const_pool.next_index += 1;
  return id;
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
