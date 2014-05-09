#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "vm.h"
#include "symbol.h"
#include "object.h"
#include "frame.h"
#include "exception.h"
#include "chunk.h"
// #include "generator.h"
#include "gc1.h"

struct hvm_obj_ref* hvm_const_null = &(hvm_obj_ref){
  .type = HVM_NULL,
  .data.v = NULL,
  .flags = HVM_OBJ_FLAG_CONSTANT
};
struct hvm_obj_ref* hvm_const_zero = &(hvm_obj_ref){
  .type = HVM_INTEGER,
  .data.i64 = 0,
  .flags = HVM_OBJ_FLAG_CONSTANT
};

char *hvm_util_strclone(char *str) {
  size_t len = strlen(str);
  char  *clone = malloc(sizeof(char) * (size_t)(len + 1));
  strcpy(clone, str);
  return clone;
}

// 1000 0000 = 0x80
unsigned char HVM_DEBUG_FLAG_HIDE_BACKTRACE = 0x80;

hvm_vm *hvm_new_vm() {
  hvm_vm *vm = malloc(sizeof(hvm_vm));
  vm->ip = 0;
  vm->program_capacity = HVM_PROGRAM_INITIAL_CAPACITY;
  vm->program_size = 0;
  vm->program = calloc(sizeof(byte), vm->program_capacity);
  vm->symbol_table = hvm_new_obj_struct();
  // Constants
  vm->const_pool.next_index = 0;
  vm->const_pool.size = HVM_CONSTANT_POOL_INITIAL_SIZE;
  vm->const_pool.entries = malloc(sizeof(struct hvm_object_ref*) * 
    vm->const_pool.size);
  // Variables
  vm->globals      = hvm_new_obj_struct();
  vm->symbols      = hvm_new_symbol_store();
  vm->primitives   = hvm_new_obj_struct();
  vm->obj_space    = hvm_new_obj_space();

  vm->stack = calloc(HVM_STACK_SIZE, sizeof(struct hvm_frame));
  vm->stack_depth = 0;
  hvm_frame_initialize(&vm->stack[0]);
  vm->root = &vm->stack[0];
  vm->top = &vm->stack[0];

  vm->exception = NULL;
  vm->debug_entries_capacity = HVM_DEBUG_ENTRIES_INITIAL_CAPACITY;
  vm->debug_entries = malloc(sizeof(hvm_chunk_debug_entry*) * vm->debug_entries_capacity);
  vm->debug_entries_size = 0;

  return vm;
}

void hvm_vm_expand_program(hvm_vm *vm) {
  vm->program_capacity = HVM_PROGRAM_GROW_FUNCTION(vm->program_capacity);
  vm->program = realloc(vm->program, sizeof(byte) * vm->program_capacity);
}

void hvm_vm_load_chunk_debug_entries(hvm_vm *vm, uint64_t start, hvm_chunk_debug_entry **entries) {
  hvm_chunk_debug_entry *de;
  while(*entries != NULL) {
    de = *entries;
    // Grow if necessary
    if(vm->debug_entries_size >= (vm->debug_entries_capacity - 1)) {
      vm->debug_entries_capacity = HVM_DEBUG_ENTRIES_GROW_FUNCTION(vm->debug_entries_capacity);
      vm->debug_entries = realloc(vm->debug_entries, sizeof(hvm_chunk_debug_entry) * vm->debug_entries_capacity);
    }
    // Copy entry
    uint64_t size = vm->debug_entries_size;
    memcpy(&vm->debug_entries[size], de, sizeof(hvm_chunk_debug_entry));
    vm->debug_entries[size].start += start;
    vm->debug_entries[size].end   += start;

    vm->debug_entries_size++;
    entries++;
  }
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
}
void hvm_vm_load_chunk_relocations(hvm_vm *vm, uint64_t start, hvm_chunk_relocation **relocs) {
  hvm_chunk_relocation *reloc;
  while(*relocs != NULL) {
    reloc = *relocs;
    uint64_t index = reloc->index;
    uint64_t dest;
    int64_t  i64_dest;

    if((start + index) >= 2) {
      uint64_t op_index = (start + index) - 2;
      byte op = vm->program[op_index];
      if(op == HVM_OP_LITINTEGER) { goto i64_reloc; }
    }
    // Get the dest.
    memcpy(&dest, &vm->program[start + index], sizeof(uint64_t));
    // Update the dest.
    dest += start;
    // Then write the dest back.
    memcpy(&vm->program[start + index], &dest, sizeof(uint64_t));
    goto tail;
  i64_reloc:
    memcpy(&i64_dest, &vm->program[start + index], sizeof(int64_t));
    i64_dest += (int64_t)start;
    memcpy(&vm->program[start + index], &i64_dest, sizeof(int64_t));
  tail:
    relocs++;
    continue;
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
  hvm_vm_load_chunk_debug_entries(vm, start, chunk->debug_entries);
}

hvm_obj_ref *hvm_vm_call_primitive(hvm_vm *vm, hvm_symbol_id sym_id) {
  hvm_obj_ref* (*prim)(hvm_vm *vm);

  // hvm_obj_print_structure(vm, vm->primitives);
  void *pv = hvm_obj_struct_internal_get(vm->primitives, sym_id);
  if(pv == NULL) {
    // TODO: Refactor exception creation
    // Primitive not found
    hvm_exception  *exc = hvm_new_exception();
    char buff[256];// TODO: Buffer overflow if primitive name too long
    buff[0] = '\0';
    strcat(buff, "Primitive not found: ");
    // NOTE: Possible error that desymbolicate() could fail.
    strcat(buff, hvm_desymbolicate(vm->symbols, sym_id));
    hvm_obj_ref *obj = hvm_new_obj_ref_string_data(hvm_util_strclone(buff));
    exc->message = obj;

    hvm_location *loc = hvm_new_location();
    loc->name = hvm_util_strclone("hvm_vm_call_primitive");
    hvm_exception_push_location(exc, loc);

    vm->exception = exc;
    return NULL;
  }
  prim = pv;
  // Invoke the actual primitive
  return prim(vm);
}

hvm_exception *hvm_new_operand_not_integer_exception() {
  hvm_exception *exc = hvm_new_exception();
  char *msg = "Operands must be integers";
  hvm_obj_ref *obj = hvm_new_obj_ref_string_data(hvm_util_strclone(msg));
  exc->message = obj;

  hvm_location *loc = hvm_new_location();
  loc->name = hvm_util_strclone("hvm_obj_int_op");
  hvm_exception_push_location(exc, loc);
  return exc;
}

hvm_obj_ref* hvm_vm_build_closure(hvm_vm *vm) {
  hvm_obj_ref *ref = hvm_new_obj_ref();
  ref->type = HVM_STRUCTURE;
  // Create the closure struct and write the whole stack into it (bottom-up)
  hvm_obj_struct *closure_struct = hvm_new_obj_struct();
  uint32_t i = 0;
  while(i <= vm->stack_depth) {
    hvm_frame *frame = &vm->stack[i];
    hvm_obj_struct *locals = frame->locals;
    unsigned int idx;
    for(idx = 0; idx < locals->heap_length; idx++) {
      // Copy entry in the scope's structure heap into the closure
      hvm_obj_struct_heap_pair *pair = locals->heap[idx];
      hvm_obj_struct_internal_set(closure_struct, pair->id, pair->obj);
    }
    // Move up the stack
    i++;
  }
  ref->data.v = closure_struct;
  return ref;
}

/*
REGISTER MAP
0-127   = General registers (128)
128     = Zero
129     = Null
130-145 = Argument registers (16)
146-161 = Parameter registers (16)
162     = Special parameter length register
*/
byte hvm_vm_reg_gen(byte i) {
  assert(i <= 127);
  return i;
}
byte hvm_vm_reg_zero() { return 128; }
byte hvm_vm_reg_null() { return 129; }
byte hvm_vm_reg_arg(byte i) {
  assert(i < HVM_ARGUMENT_REGISTERS);
  return 130 + i;
}
byte hvm_vm_reg_param(byte i) {
  assert(i < HVM_PARAMETER_REGISTERS);
  return 146 + i;
}

__attribute__((always_inline)) void hvm_vm_register_write(hvm_vm *vm, byte reg, hvm_obj_ref* ref) {
  assert(reg < 146 || reg > 161);// Avoid parameter registers
  if(reg <= 127) {
    vm->general_regs[reg] = ref;
  } else if(reg >= 130 && reg <= 145) {
    vm->arg_regs[reg - 130] = ref;
  }
  // Else noop
}
__attribute__((always_inline)) hvm_obj_ref* hvm_vm_register_read(hvm_vm *vm, byte reg) {
  assert(reg < 130 || reg > 145);// Avoid argument registers
  assert(reg <= 162);// Valid range of registers
  if(reg <= 127) {
    return vm->general_regs[reg];
  }
  if(reg >= 146 && reg <= 162) {
    return vm->param_regs[reg - 146];
  }
  if(reg == 128) { return hvm_const_zero; }
  if(reg == 129) { return hvm_const_null; }
  // Should never reach here.
  return NULL;
}

__attribute__((always_inline)) void hvm_vm_copy_regs(hvm_vm *vm) {
  int64_t i;
  // Reset params
  for(i = 0; i < HVM_PARAMETER_REGISTERS; i++) { vm->param_regs[i] = NULL; }
  // Copy args into params
  i = 0;
  while(i < HVM_ARGUMENT_REGISTERS && vm->arg_regs[i] != NULL) {
    // Copy and null the source
    vm->param_regs[i] = vm->arg_regs[i];
    vm->arg_regs[i] = NULL;
  }
  // Set special $pn.
  hvm_obj_ref *pn = hvm_new_obj_int();
  pn->data.i64 = i + 1;
  vm->param_regs[HVM_PARAMETER_REGISTERS - 1] = pn;
}


#define READ_U32(V) *(uint32_t*)(V)
#define READ_U64(V) *(uint64_t*)(V)
#define READ_I32(V) *(int32_t*)(V)
#define READ_I64(V) *(int64_t*)(V)

#define AREG areg = vm->program[vm->ip + 1];
#define BREG breg = vm->program[vm->ip + 2];
#define CREG creg = vm->program[vm->ip + 3];

#define CHECK_EXCEPTION if(vm->exception != NULL) { goto handle_exception; }

void hvm_vm_run(hvm_vm *vm) {
  byte instr;
  uint32_t const_index;
  uint64_t dest, sym_id;//, return_addr;
  int32_t diff;
  int64_t i64;
  unsigned char reg, areg, breg, creg;
  hvm_obj_ref *a, *b, *c, *arr, *idx, *key, *val, *strct;
  hvm_frame *frame, *parent_frame;
  hvm_exception *exc;

execute:
  for(; vm->ip < vm->program_size;) {
    // fprintf(stderr, "top: %p, ip: %llu\n", vm->top, vm->ip);
    // Update the current frame address
    vm->top->current_addr = vm->ip;
    // Fetch the instruction
    instr = vm->program[vm->ip];
    switch(instr) {
      case HVM_OP_NOOP:
        // fprintf(stderr, "NOOP\n");
        break;
      case HVM_OP_DIE:
        // fprintf(stderr, "DIE\n");
        goto end;
      case HVM_OP_TAILCALL: // 1B OP | 8B DEST
        dest = READ_U64(&vm->program[vm->ip + 1]);
        // Copy important bits from parent.
        parent_frame = vm->top;
        uint64_t parent_ret_addr = parent_frame->return_addr;
        byte     parent_ret_reg  = parent_frame->return_register;
        // Overwrite current frame (ie. parent).
        frame = &vm->stack[vm->stack_depth];
        hvm_frame_initialize(frame);
        frame->return_addr     = parent_ret_addr;
        frame->return_register = parent_ret_reg;
        hvm_vm_copy_regs(vm);
        vm->ip = dest;
        continue;
      case HVM_OP_CALL: // 1B OP | 8B DEST | 1B REG
        dest = READ_U64(&vm->program[vm->ip + 1]);
        reg  = vm->program[vm->ip + 9];
        vm->stack_depth += 1;
        frame = &vm->stack[vm->stack_depth];
        hvm_frame_initialize(frame);
        frame->return_addr     = vm->ip + 10; // Instruction is 10 bytes long.
        frame->return_register = reg;
        hvm_vm_copy_regs(vm);
        vm->ip = dest;
        vm->top = frame;
        continue;
      case HVM_OP_CALLSYMBOLIC:// 1B OP | 1B REG | 1B REG
        AREG; BREG;
        key = hvm_vm_register_read(vm, areg);// This is the symbol we need to look up.
        assert(key->type == HVM_SYMBOL);
        sym_id = key->data.u64;
        // fprintf(stderr, "0x%08llX  ", vm->ip);
        // fprintf(stderr, "sym: %llu -> %s\n", sym_id, hvm_desymbolicate(vm->symbols, sym_id));
        // hvm_obj_print_structure(vm, vm->symbol_table);
        val    = hvm_obj_struct_internal_get(vm->symbol_table, sym_id);
        // fprintf(stderr, "val: %p\n", val);
        dest   = val->data.u64;
        assert(val->type == HVM_INTERNAL);
        // fprintf(stderr, "CALLSYMBOLIC(0x%08llX, $%d)\n", dest, breg);
        vm->stack_depth += 1;
        frame = &vm->stack[vm->stack_depth];
        hvm_frame_initialize(frame);
        frame->return_addr = vm->ip + 3;// Instruction is 3 bytes long
        frame->return_register = breg;
        hvm_vm_copy_regs(vm);
        vm->ip = dest;
        vm->top = frame;
        continue;
      case HVM_OP_CATCH: // 1B OP | 8B DEST | 1B REG
        dest = READ_U64(&vm->program[vm->ip + 1]);
        reg  = vm->program[vm->ip + 9];
        frame = &vm->stack[vm->stack_depth];
        frame->catch_addr     = dest;
        frame->catch_register = reg;
        vm->ip += 9;
        break;
      case HVM_OP_CLEARCATCH: // 1B OP
        frame = &vm->stack[vm->stack_depth];
        frame->catch_addr     = HVM_FRAME_EMPTY_CATCH;
        frame->catch_register = hvm_vm_reg_null();
        break;
      case HVM_OP_CLEAREXCEPTION: // 1B OP
        vm->exception = NULL;
        break;
      case HVM_OP_SETEXCEPTION: // 1B OP | 1B REG
        reg = vm->program[vm->ip + 1];
        // TODO: Throw new exception if no current exception.
        assert(vm->exception != NULL);
        val = hvm_obj_for_exception(vm, vm->exception);
        hvm_vm_register_write(vm, reg, val);
        vm->ip += 1;
        break;
        
      case HVM_OP_CALLADDRESS: // 1B OP | 1B REG | 1B REG
        reg  = vm->program[vm->ip + 1];
        val  = hvm_vm_register_read(vm, reg);
        assert(val->type == HVM_INTEGER);
        dest = (uint64_t)val->data.i64;
        reg  = vm->program[vm->ip + 2]; // Return register now
        vm->stack_depth += 1;
        frame = &vm->stack[vm->stack_depth];
        hvm_frame_initialize(frame);
        frame->return_addr     = vm->ip + 3; // Instruction 3 bytes long.
        frame->return_register = reg;
        vm->ip = dest;
        vm->ip = dest;
        vm->top = frame;
        continue;
      case HVM_OP_RETURN: // 1B OP | 1B REG
        reg = vm->program[vm->ip + 1];
        // Current frame
        frame = vm->top;
        vm->ip = frame->return_addr;
        vm->stack_depth -= 1;
        vm->top = &vm->stack[vm->stack_depth];
        hvm_vm_register_write(vm, frame->return_register, hvm_vm_register_read(vm, reg));
        // fprintf(stderr, "RETURN(0x%08llX) $%d -> $%d\n", frame->return_addr, reg, frame->return_register);
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
      case HVM_OP_GOTOADDRESS: // 1B OP | 1B REGDEST
        reg = vm->program[vm->ip + 1];
        val  = hvm_vm_register_read(vm, reg);
        assert(val->type == HVM_INTEGER);
        i64  = val->data.i64;
        dest = (uint64_t)i64;
        vm->ip = dest;
        // fprintf(stderr, "GOTOADDRESS(0x%08llX)\n", dest);
        continue;
      case HVM_OP_IF: // 1B OP | 1B REG  | 8B DEST
        reg  = vm->program[vm->ip + 1];
        dest = READ_U64(&vm->program[vm->ip + 2]);
        val  = hvm_vm_register_read(vm, reg);
        if(val->type == HVM_NULL || (val->type == HVM_INTEGER && val->data.i64 == 0)) {
          // Falsey, add on the 9 bytes for the instruction parameters and continue onwards.
          vm->ip += 9;
          break;
        } else {
          // Truthy, go straight to destination.
          vm->ip = dest;
          continue;
        }
      case HVM_OP_CALLPRIMITIVE: // 1B OP | 1B REG | 1B REG
        AREG; BREG;
        key = hvm_vm_register_read(vm, areg);// This is the symbol we need to look up.
        assert(key->type == HVM_SYMBOL);
        sym_id = key->data.u64;
        hvm_vm_copy_regs(vm);
        // fprintf(stderr, "CALLPRIMITIVE(%lld, $%d)\n", sym_id, breg);
        // FIXME: This may need to be smartened up.
        hvm_exception *current_exc = vm->exception;// If there's a current exception
        val = hvm_vm_call_primitive(vm, sym_id);
        // CHECK_EXCEPTION;
        if(vm->exception != current_exc) { goto handle_exception; }
        hvm_vm_register_write(vm, breg, val);
        vm->ip += 2;
        break;

      case HVM_OP_LITINTEGER: // 1B OP | 1B REG | 8B LIT
        reg = vm->program[vm->ip + 1];
        i64 = READ_I64(&vm->program[vm->ip + 2]);
        val = hvm_new_obj_int();
        val->data.i64 = i64;
        hvm_obj_space_add_obj_ref(vm->obj_space, val);
        hvm_vm_register_write(vm, reg, val);
        vm->ip += 9;
        break;

      case HVM_OP_MOVE: // 1B OP | 1B REG | 1B REG
        AREG; BREG;
        hvm_vm_register_write(vm, areg, hvm_vm_register_read(vm, breg));
        vm->ip += 2;
        break;

      case HVM_OP_SETSTRING:  // 1 = reg, 2-5 = const
      case HVM_OP_SETINTEGER: // 1B OP | 1B REG | 4B CONST
      case HVM_OP_SETFLOAT:
      case HVM_OP_SETSTRUCT:
      case HVM_OP_SETSYMBOL:
        // TODO: Type-checking or just do SETCONSTANT
        reg         = vm->program[vm->ip + 1];
        const_index = READ_U32(&vm->program[vm->ip + 2]);
        // fprintf(stderr, "0x%08llX  ", vm->ip);
        // fprintf(stderr, "SET $%u = const(%u)\n", reg, const_index);
        hvm_vm_register_write(vm, reg, hvm_vm_get_const(vm, const_index));
        vm->ip += 5;
        break;
      case HVM_OP_SETNULL: // 1B OP | 1B REG
        reg = vm->program[vm->ip + 1];
        hvm_vm_register_write(vm, reg, hvm_const_null);
        vm->ip += 1;
        break;

      // case HVM_OP_SETSYMBOL: // 1B OP | 1B REG | 4B CONST
      //   reg = vm->program[vm->ip + 1];
      //   const_index = READ_U32(&vm->program[vm->ip + 2]);
      //   vm->general_regs[reg] = hvm_vm_get_const(vm, const_index);

      case HVM_OP_SETLOCAL: // 1B OP | 1B REG   | 1B REG (local(A) = B)
        AREG; BREG;
        key = hvm_vm_register_read(vm, areg);
        assert(key->type == HVM_SYMBOL);
        hvm_set_local(vm->top, key->data.u64, hvm_vm_register_read(vm, breg));
        vm->ip += 2;
        break;
      case HVM_OP_GETLOCAL: // 1B OP | 1B REG   | 1B REG (A = local(B))
        AREG; BREG;
        key = hvm_vm_register_read(vm, breg);
        assert(key->type == HVM_SYMBOL);
        val = hvm_get_local(vm->top, key->data.u64);
        if(val == NULL) {
          // Local not found
          hvm_exception *exc = hvm_new_exception();
          char buff[256];// TODO: Danger, Will Robinson, buffer overflow!
          buff[0] = '\0';
          strcat(buff, "Undefined local: ");
          strcat(buff, hvm_desymbolicate(vm->symbols, key->data.u64));
          hvm_obj_ref *obj = hvm_new_obj_ref_string_data(hvm_util_strclone(buff));
          exc->message = obj;

          vm->exception = exc;
          goto handle_exception;
          // val = hvm_const_null;
        }
        hvm_vm_register_write(vm, areg, val);
        vm->ip += 2;
        break;

      case HVM_OP_SETGLOBAL: // 1B OP | 1B REG   | 1B REG
        AREG; BREG;
        key = hvm_vm_register_read(vm, areg);
        assert(key->type == HVM_SYMBOL);
        hvm_set_global(vm, key->data.u64, hvm_vm_register_read(vm, breg));
        vm->ip += 2;
        break;
      case HVM_OP_GETGLOBAL: // 1B OP | 1B REG   | 1B SYM
        AREG; BREG;
        key = hvm_vm_register_read(vm, breg);
        assert(key->type == HVM_SYMBOL);
        hvm_vm_register_write(vm, areg, hvm_get_global(vm, key->data.u64));
        vm->ip += 2;
        break;

      case HVM_OP_GETCLOSURE: // 1B OP | 1B REG
        reg = vm->program[vm->ip + 1];
        // hvm_obj_ref* ref = hvm_new_obj_ref();
        // ref->type = HVM_STRUCTURE;
        // ref->data.v = vm->top->locals;
        hvm_obj_ref *ref = hvm_vm_build_closure(vm);
        hvm_obj_space_add_obj_ref(vm->obj_space, ref);
        hvm_vm_register_write(vm, reg, ref);
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
        b = hvm_vm_register_read(vm, breg);
        c = hvm_vm_register_read(vm, creg);
        // TODO: Add float support
        if(instr == HVM_OP_ADD)      { a = hvm_obj_int_add(b, c); }
        else if(instr == HVM_OP_SUB) { a = hvm_obj_int_sub(b, c); }
        else if(instr == HVM_OP_MUL) { a = hvm_obj_int_mul(b, c); }
        else if(instr == HVM_OP_DIV) { a = hvm_obj_int_div(b, c); }
        else if(instr == HVM_OP_MOD) { a = hvm_obj_int_mod(b, c); }
        if(a == NULL) {
          // Bad type
          hvm_exception *exc = hvm_new_operand_not_integer_exception();
          vm->exception = exc;
          goto handle_exception;
        }
        // Ensure the resulting integer is tracked in the GC
        hvm_obj_space_add_obj_ref(vm->obj_space, a);
        hvm_vm_register_write(vm, areg, a);
        vm->ip += 3;
        break;

      // MATHEMATICAL COMPARISON ----------------------------------------------
      case HVM_OP_LT:
      case HVM_OP_GT:
      case HVM_OP_LTE:
      case HVM_OP_GTE:
      case HVM_OP_EQ:  // 1B OP | 3B REGs
        // A = B < C
        AREG; BREG; CREG;
        b = hvm_vm_register_read(vm, breg);
        c = hvm_vm_register_read(vm, creg);
        if(instr == HVM_OP_LT)       { a = hvm_obj_int_lt(b, c); }
        else if(instr == HVM_OP_GT)  { a = hvm_obj_int_gt(b, c); }
        else if(instr == HVM_OP_LTE) { a = hvm_obj_int_lte(b, c); }
        else if(instr == HVM_OP_GTE) { a = hvm_obj_int_gte(b, c); }
        else if(instr == HVM_OP_EQ) { a = hvm_obj_int_eq(b, c); }
        if(a == NULL) {
          hvm_exception *exc = hvm_new_operand_not_integer_exception();
          vm->exception = exc;
          goto handle_exception;
        }
        hvm_obj_space_add_obj_ref(vm->obj_space, a);
        hvm_vm_register_write(vm, areg, a);
        vm->ip += 3;
        break;

      // ARRAYS ---------------------------------------------------------------
      case HVM_OP_ARRAYPUSH: // 1B OP | 2B REGS
        // A.push(B)
        AREG; BREG;
        a = hvm_vm_register_read(vm, areg);
        b = hvm_vm_register_read(vm, breg);
        hvm_obj_array_push(a, b);
        vm->ip += 2;
        break;
      case HVM_OP_ARRAYUNSHIFT: // 1B OP | 2B REGS
        // A.unshift(B)
        AREG; BREG;
        a = hvm_vm_register_read(vm, areg);
        b = hvm_vm_register_read(vm, breg);
        hvm_obj_array_unshift(a, b);
        vm->ip += 2;
        break;
      case HVM_OP_ARRAYSHIFT: // 1B OP | 2B REGS
        // A = B.shift()
        AREG; BREG;
        b = hvm_vm_register_read(vm, breg);
        hvm_vm_register_write(vm, areg, hvm_obj_array_shift(b));
        vm->ip += 2;
        break;
      case HVM_OP_ARRAYPOP: // 1B OP | 2B REGS
        // A = B.pop()
        AREG; BREG;
        b = hvm_vm_register_read(vm, breg);
        hvm_vm_register_write(vm, areg, hvm_obj_array_pop(b));
        vm->ip += 2;
        break;
      case HVM_OP_ARRAYGET: // 1B OP | 3B REGS
        // arrayget V A I -> V = A[I]
        AREG; BREG; CREG;
        arr = hvm_vm_register_read(vm, breg);
        idx = hvm_vm_register_read(vm, creg);
        hvm_vm_register_write(vm, areg, hvm_obj_array_get(arr, idx));
        vm->ip += 3;
        break;
      case HVM_OP_ARRAYSET: // 1B OP | 3B REGS
        // arrayset A I V -> A[I] = V
        AREG; BREG; CREG;
        arr = hvm_vm_register_read(vm, areg);
        idx = hvm_vm_register_read(vm, breg);
        val = hvm_vm_register_read(vm, creg);
        hvm_obj_array_set(arr, idx, val);
        vm->ip += 3;
        break;
      case HVM_OP_ARRAYREMOVE: // 1B OP | 3B REGS
        // arrayremove V A I
        AREG; BREG; CREG;
        arr = hvm_vm_register_read(vm, breg);
        idx = hvm_vm_register_read(vm, creg);
        hvm_vm_register_write(vm, areg, hvm_obj_array_remove(arr, idx));
        vm->ip += 3;
        break;
      case HVM_OP_ARRAYNEW: // 1B OP | 2B REGS
        // arraynew A L
        AREG; BREG;
        val = hvm_vm_register_read(vm, breg);
        hvm_obj_array *arr = hvm_new_obj_array_with_length(val);
        a = hvm_new_obj_ref();
        a->type = HVM_ARRAY;
        a->data.v = arr;
        hvm_obj_space_add_obj_ref(vm->obj_space, a);
        hvm_vm_register_write(vm, areg, a);
        vm->ip += 2;
        break;

      // STRUCTS --------------------------------------------------------------
      case HVM_OP_STRUCTSET:
        // structset S K V
        AREG; BREG; CREG;
        strct = hvm_vm_register_read(vm, areg);
        key   = hvm_vm_register_read(vm, breg);
        val   = hvm_vm_register_read(vm, creg);
        hvm_obj_struct_set(strct, key, val);
        // fprintf(stderr, "0x%08llX  ", vm->ip);
        // fprintf(stderr, "STRUCTSET $%u = $%u[$%u(%llu)]\n", areg, breg, creg, key->data.u64);
        // hvm_obj_print_structure(vm, strct->data.v);
        vm->ip += 3;
        break;
      case HVM_OP_STRUCTGET:
        // structget V S K
        AREG; BREG; CREG;
        // fprintf(stderr, "0x%08llX  ", vm->ip);
        // fprintf(stderr, "STRUCTGET $%u = $%u[$%u(%llu)]\n", areg, breg, creg, key->data.u64);
        strct = hvm_vm_register_read(vm, breg);
        key   = hvm_vm_register_read(vm, creg);
        if(strct->type != HVM_STRUCTURE) {
          // Bad type
          hvm_exception *exc = hvm_new_exception();
          char *msg = "Attempting to get member of non-structure";
          hvm_obj_ref *obj = hvm_new_obj_ref_string_data(hvm_util_strclone(msg));
          exc->message = obj;

          hvm_location *loc = hvm_new_location();
          loc->name = hvm_util_strclone("hvm_structget");
          hvm_exception_push_location(exc, loc);

          vm->exception = exc;
          goto handle_exception;
        }
        assert(strct->type == HVM_STRUCTURE);
        assert(key->type == HVM_SYMBOL);
        // hvm_obj_print_structure(vm, strct->data.v);
        val = hvm_obj_struct_get(strct, key);
        assert(val != NULL);
        hvm_vm_register_write(vm, areg, val);
        vm->ip += 3;
        break;
      case HVM_OP_STRUCTDELETE:
        // structdelete V S K`
        AREG; BREG; CREG;
        strct = hvm_vm_register_read(vm, breg);
        key   = hvm_vm_register_read(vm, creg);
        hvm_vm_register_write(vm, areg, hvm_obj_struct_delete(strct, key));
        vm->ip += 3;
        break;
      case HVM_OP_STRUCTNEW:
        // structnew S`
        AREG;
        hvm_obj_struct *s = hvm_new_obj_struct();
        strct = hvm_new_obj_ref();
        strct->type = HVM_STRUCTURE;
        strct->data.v = s;
        hvm_obj_space_add_obj_ref(vm->obj_space, strct);
        hvm_vm_register_write(vm, areg, strct);
        vm->ip += 1;
        break;
      case HVM_OP_STRUCTHAS:
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
handle_exception:
  exc = vm->exception;
  assert(exc != NULL);
  hvm_exception_build_backtrace(exc, vm);
  // Climb stack looking for catch handler.
  uint32_t depth = vm->stack_depth;
  while(1) {
    frame = &vm->stack[depth];
    if(frame->catch_addr != HVM_FRAME_EMPTY_CATCH) {
      val = hvm_obj_for_exception(vm, exc);
      hvm_vm_register_write(vm, frame->catch_register, val);
      // Resume execution at the exception handling address
      vm->ip = frame->catch_addr;
      // Clear the exception handler from the frame
      frame->catch_addr = HVM_FRAME_EMPTY_CATCH;
      frame->catch_register = hvm_vm_reg_null();
      goto execute;
      return;
    }
    if(depth == 0) { break; }
    depth--;
  }
  // No exception handler found
  hvm_exception_print(exc);
  return;
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
