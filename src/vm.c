#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
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
#include "debug.h"
#include "jit-tracer.h"
#include "jit-compiler.h"

#ifndef bool
#define bool char
#endif

// Prefix a function definition with this to force its inling into caller
#define ALWAYS_INLINE __attribute__((always_inline))

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
  // Registers
  for(unsigned int i = 0; i < HVM_GENERAL_REGISTERS; i++) {
    vm->general_regs[i] = hvm_const_null;
  }
  for(unsigned int i = 0; i < HVM_ARGUMENT_REGISTERS; i++) {
    vm->arg_regs[i] = hvm_const_null;
  }
  for(unsigned int i = 0; i < HVM_PARAMETER_REGISTERS; i++) {
    vm->param_regs[i] = hvm_const_null;
  }
  // Constants
  vm->const_pool.next_index = 0;
  vm->const_pool.size = HVM_CONSTANT_POOL_INITIAL_SIZE;
  vm->const_pool.entries = malloc(sizeof(hvm_obj_ref*) * vm->const_pool.size);
  // Variables
  vm->globals    = hvm_new_obj_struct();
  vm->symbols    = hvm_new_symbol_store();
  vm->primitives = hvm_new_obj_struct();

  // Setup allocator and garbage collector
  vm->obj_space = hvm_new_obj_space();
  vm->ref_pool  = hvm_obj_ref_pool_new();

  vm->stack = calloc(HVM_STACK_SIZE, sizeof(struct hvm_frame));
  vm->stack_depth = 0;
  hvm_frame_initialize(&vm->stack[0]);
  vm->root = &vm->stack[0];
  vm->top = &vm->stack[0];

  vm->exception = NULL;
  vm->debug_entries_capacity = HVM_DEBUG_ENTRIES_INITIAL_CAPACITY;
  vm->debug_entries_size = 0;
  vm->debug_entries = malloc(sizeof(hvm_chunk_debug_entry) * vm->debug_entries_capacity);

#ifdef HVM_VM_DEBUG
  hvm_debug_setup(vm);
#endif

  vm->is_tracing = 0;
  vm->traces_length = 0;

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

hvm_obj_ref *hvm_vm_call_primitive(hvm_vm *vm, hvm_obj_ref *sym_object) {
  hvm_obj_ref* (*prim)(hvm_vm *vm);

  assert(sym_object->type == HVM_SYMBOL);
  hvm_symbol_id sym_id = sym_object->data.u64;

  // hvm_obj_print_structure(vm, vm->primitives);
  void *pv = hvm_obj_struct_internal_get(vm->primitives, sym_id);
  if(pv == NULL) {
    // TODO: Refactor exception creation
    // Primitive not found
    char buff[256];// TODO: Buffer overflow if primitive name too long
    buff[0] = '\0';
    strcat(buff, "Primitive not found: ");
    // NOTE: Possible error that desymbolicate() could fail.
    strcat(buff, hvm_desymbolicate(vm->symbols, sym_id));
    hvm_obj_ref *message = hvm_new_obj_ref_string_data(hvm_util_strclone(buff));
    hvm_obj_ref *exc = hvm_exception_new(vm, message);

    hvm_location *loc = hvm_new_location();
    loc->name = hvm_util_strclone("hvm_vm_call_primitive");
    hvm_exception_push_location(vm, exc, loc);

    vm->exception = exc;
    return NULL;
  }
  prim = pv;
  // Invoke the actual primitive
  return prim(vm);
}

hvm_obj_ref *hvm_new_operand_not_integer_exception(hvm_vm *vm) {
  char *msg = "Operands must be integers";
  hvm_obj_ref *message = hvm_new_obj_ref_string_data(hvm_util_strclone(msg));
  hvm_obj_ref *exc = hvm_exception_new(vm, message);

  hvm_location *loc = hvm_new_location();
  loc->name = hvm_util_strclone("hvm_obj_int_op");
  hvm_exception_push_location(vm, exc, loc);
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

bool hvm_is_gen_reg(byte i) {
  return i <= 127;
}
bool hvm_is_arg_reg(byte i) {
  return i >= HVM_REG_ARG_OFFSET && i < (HVM_REG_ARG_OFFSET + HVM_ARGUMENT_REGISTERS);
}
bool hvm_is_param_reg(byte i) {
  return i >= HVM_REG_PARAM_OFFSET && i < (HVM_REG_PARAM_OFFSET + HVM_ARGUMENT_REGISTERS);
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
  assert(hvm_is_gen_reg(i));
  return i;
}
byte hvm_vm_reg_zero() { return HVM_REG_ZERO; }
byte hvm_vm_reg_null() { return HVM_REG_NULL; }
byte hvm_vm_reg_arg(byte i) {
  assert(i < HVM_ARGUMENT_REGISTERS);
  return HVM_REG_ARG_OFFSET + i;
}
byte hvm_vm_reg_param(byte i) {
  assert(i < HVM_PARAMETER_REGISTERS);
  return 146 + i;
}


// Reading and writing registers
ALWAYS_INLINE void hvm_vm_register_write(hvm_vm *vm, byte reg, hvm_obj_ref* ref) {
  assert(reg < 146 || reg > 161);// Avoid parameter registers
  if(reg <= 127) {
    vm->general_regs[reg] = ref;
  } else if(reg >= 130 && reg <= 145) {
    vm->arg_regs[reg - 130] = ref;
  }
  // Else noop
}
/// Inlined version of register read operation
ALWAYS_INLINE hvm_obj_ref* _hvm_vm_register_read(hvm_vm *vm, byte reg) {
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
  assert(false);
  return NULL;
}

// External copy
hvm_obj_ref *hvm_vm_register_read(hvm_vm *vm, byte reg) {
  return _hvm_vm_register_read(vm, reg);
}

// Copy argument registers into parameter registers.
void hvm_vm_copy_regs(hvm_vm *vm) {
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
  // Set special $pn register.
  // TODO: Make sure this works with the object space properly so that it
  //       doesn't get leaked.
  hvm_obj_ref *pn = hvm_new_obj_int();
  pn->data.i64 = i + 1;
  vm->param_regs[HVM_PARAMETER_REGISTERS - 1] = pn;
}

// Utility function for setting up new stack frames
ALWAYS_INLINE void hvm_frame_initialize_returning(hvm_frame *frame, uint64_t return_addr, byte return_register) {
  // Initialize all the parts of the frame
  hvm_frame_initialize(frame);
  // Then set our return register and addresses
  frame->return_addr     = return_addr;
  frame->return_register = return_register;
}

typedef enum {
  HVM_DISPATCH_PATH_NORMAL,
  HVM_DISPATCH_PATH_JIT
} hvm_dispatch_path;

// Handle dispatching to JIT path if appropriate
ALWAYS_INLINE hvm_dispatch_path hvm_dispatch_frame(hvm_vm *vm, hvm_frame *frame, hvm_subroutine_tag *tag, byte *caller_tag) {
  uint64_t dest = vm->ip;
  hvm_call_trace *trace;
  // Check if we've reached the heat threshold
  if(tag->heat > HVM_TRACE_THRESHOLD || vm->always_trace) {
    // See if we have a completed trace available to compile and switch to
    if(tag->trace_index > 0) {
      // .trace_index is offset by one so that we can use 0 to mean
      // no-trace-exists.
      trace = vm->traces[tag->trace_index - 1];
      // Guard that the trace really is completed
      assert(trace->complete);
      // If we don't already have a compiled function then compile it
      if(trace->compiled_function == NULL) {
        hvm_jit_compile_trace(vm, trace);
        fprintf(stderr, "compiled trace for 0x%08llX\n", dest);
      }
      fprintf(stderr, "running compiled trace for 0x%08llX\n", dest);
      hvm_jit_exit *result = hvm_jit_run_compiled_trace(vm, trace);
      if(result->ret.status == HVM_JIT_EXIT_BAILOUT) {
        // If it's a bailout then we need to return to normal execution
        vm->ip = result->bailout.destination;
        return HVM_DISPATCH_PATH_NORMAL;
      } else {
        assert(vm->stack_depth != 0);
        // Otherwise it was a successful execution so pop off our frame and
        // return to the caller
        vm->ip = frame->return_addr;
        vm->stack_depth -= 1;
        vm->top = &vm->stack[vm->stack_depth];
        hvm_vm_register_write(vm, frame->return_register, result->ret.value);
        return HVM_DISPATCH_PATH_NORMAL;
      }
    }
    // fprintf(stderr, "subroutine %s:0x%08llX has heat %d\n", sym_name, dest, tag.heat);
    // Check if we need to start tracing
    if(!vm->is_tracing) {
      // If frame is already being traced
      if(frame->trace != NULL) {
        return HVM_DISPATCH_PATH_JIT;
      }
      fprintf(stderr, "switching to trace dispatch for 0x%08llX\n", dest);
      trace = hvm_new_call_trace(vm);
      trace->caller_tag = caller_tag;
      frame->trace = trace;
      vm->is_tracing = 1;
      return HVM_DISPATCH_PATH_JIT;
    }
  }
  return HVM_DISPATCH_PATH_NORMAL;
}

// Utility macro for properly dispatching a dispatch-path to the
// regular dispatcher or the tracing-for-JIT dispatcher
#define DISPATCH_PATH(DP)          \
  switch(DP) {                     \
    case HVM_DISPATCH_PATH_NORMAL: \
    goto EXECUTE;                  \
    case HVM_DISPATCH_PATH_JIT:    \
    goto EXECUTE_JIT;              \
  }

// Utilities for reading bytes as arbitrary types from addresses
#define READ_U32(V) *(uint32_t*)(V)
#define READ_U64(V) *(uint64_t*)(V)
#define READ_I32(V) *(int32_t*)(V)
#define READ_I64(V) *(int64_t*)(V)


#define READ_TAG            hvm_subroutine_read_tag(&vm->program[vm->ip + 1], &tag);
#define WRITE_TAG           hvm_subroutine_write_tag(&vm->program[vm->ip + 1], &tag);
#define INCREMENT_TAG_HEAT  if(tag.heat != 1024) { tag.heat += 1; }

// Generic tag handler
#define PROCESS_TAG { \
  READ_TAG; \
  INCREMENT_TAG_HEAT; \
  WRITE_TAG; \
}


#define AREG areg = vm->program[vm->ip + 1];
#define BREG breg = vm->program[vm->ip + 2];
#define CREG creg = vm->program[vm->ip + 3];

#define CHECK_EXCEPTION if(vm->exception != NULL) { goto handle_exception; }

void hvm_vm_run(hvm_vm *vm) {
  byte instr;
  byte *caller_tag;
  uint32_t const_index, depth;
  uint64_t dest, sym_id;//, return_addr;
  int32_t diff;
  int64_t i64;
  unsigned char reg, areg, breg, creg;
  hvm_obj_ref *a, *b, *c, *arr, *idx, *key, *val, *strct;
  hvm_frame *frame, *parent_frame;
  // hvm_exception *exc;
  hvm_obj_ref *exc;
  char *msg;
  hvm_subroutine_tag tag;
  // hvm_call_trace *trace;
  // Variables needed by the debugger
  #ifdef HVM_VM_DEBUG
    bool should_continue;
  #endif

// Plain dispatch loop
#include "vm-dispatch.include.c"

// JIT dispatch loop with hooks into tracing
#define JIT_DISPATCH
#include "vm-dispatch.include.c"

end:
  return;
}

#if !defined(__LITTLE_ENDIAN__) && !defined(__BIG_ENDIAN__)
#error Hivm requires a defined byte order
#endif

// TODO: Deal with endianness (right now this is little-endian-only)
#if defined(__LITTLE_ENDIAN__)
uint32_t _hvm_tag_read_endian(byte *tag_start) {
  // Strip off the first byte
  uint32_t raw = READ_U32(tag_start) & 0x00FFFFFF;
  return raw;
}
void _hvm_tag_write_endian(byte *tag_start, uint32_t value) {
  // Get the current 4 bytes (with the 3 bytes for the new tag zeroed out)
  uint32_t source = READ_U32(tag_start) & 0xFF000000;
  // Compute the 4 bytes to write
  uint32_t write = value | source;
  // Then write them back
  *(uint32_t*)tag_start = write;
}
#else
#error Hivm subroutine tagging is currently little-endian only
#endif

void hvm_subroutine_read_tag(byte *tag_start, hvm_subroutine_tag *tag) {
  uint32_t raw = _hvm_tag_read_endian(tag_start);
  // Get the top 8 bits for the heat field
  uint32_t heat = (raw & 0x00FF0000) >> 16;
  tag->heat   = (unsigned short)heat;
  // Then the bottom 16 are for the trace index
  tag->trace_index = raw & 0x0000FFFF;
  // fprintf(stderr, "read:  tag->heat = %u\n", tag->heat);
}
void hvm_subroutine_write_tag(byte *tag_start, hvm_subroutine_tag *tag) {
  // Shift the head over 16 bits so it will be in the top 8 bits
  uint32_t heat = (uint32_t)(tag->heat) << 16;
  // The trace index is fine in place (will be in the lower bits)
  uint32_t trace_index = (uint32_t)(tag->trace_index);
  // Build up the raw (with the highest byte cleared out for safety)
  uint32_t value = (heat | trace_index) & 0x00FFFFFF;
  _hvm_tag_write_endian(tag_start, value);
  // fprintf(stderr, "write: tag->heat = %u\n", tag->heat);
  // fprintf(stderr, "raw: 0x%08X\n", raw);
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
