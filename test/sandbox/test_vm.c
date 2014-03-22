#include <stdio.h>
#include <assert.h>

#include "hvm.h"
#include "hvm_symbol.h"
#include "hvm_object.h"
#include "hvm_chunk.h"
#include "hvm_generator.h"
#include "hvm_bootstrap.h"

/*
void test_heap() {
  hvm_obj_string *str = hvm_new_obj_string();
  str->data = "test";
  hvm_obj_ref *ref = hvm_new_obj_ref();
  hvm_obj_ref_set_string(ref, str);

  hvm_obj_string *str2;
  hvm_obj_ref    *ref2;;

  hvm_obj_struct *s = hvm_new_obj_struct();
  hvm_obj_ref *sref = hvm_new_obj_ref();
  sref->type = HVM_STRUCTURE;
  sref->data.v = s;
  
  hvm_obj_ref *sym = hvm_new_obj_ref();
  sym->type = HVM_SYMBOL;
  sym->data.u64 = 0;
  hvm_obj_struct_set(sref, sym, ref);
  
  ref2 = hvm_obj_struct_get(sref, sym);
  str2 = ref2->data.v;
  printf("str2->data: %s\n", str2->data);
}
*/


void test_generator() {
  hvm_gen *gen = hvm_new_gen();
  // hvm_gen_set_symbol(gen->block, hvm_vm_reg_gen(1), "_test");
  // hvm_gen_callsymbolic(gen->block, hvm_vm_reg_gen(1), hvm_vm_reg_gen(2));
  // hvm_gen_die(gen->block);
  // hvm_gen_sub(gen->block, "_test");
  // hvm_gen_goto_label(gen->block, "label");
  // hvm_gen_label(gen->block, "label");
  // hvm_gen_litinteger(gen->block, hvm_vm_reg_gen(3), 1);
  // hvm_gen_litinteger(gen->block, hvm_vm_reg_gen(4), 2);
  // hvm_gen_add(gen->block, hvm_vm_reg_gen(5), hvm_vm_reg_gen(3), hvm_vm_reg_gen(4));
  // hvm_gen_return(gen->block, hvm_vm_reg_gen(5));
  
  // OLD TEST
  /*
  hvm_gen_set_string(gen->block, hvm_vm_reg_arg(0), "Hello world!\n");
  hvm_gen_set_symbol(gen->block, hvm_vm_reg_gen(0), "print");
  hvm_gen_callprimitive(gen->block, hvm_vm_reg_gen(0), hvm_vm_reg_null());
  
  hvm_gen_set_symbol(gen->block, hvm_vm_reg_gen(0), "exit");
  hvm_gen_callprimitive(gen->block, hvm_vm_reg_gen(0), hvm_vm_reg_null());
  // hvm_gen_die(gen->block);
  */
  
  byte obj, func, sym, arg_sym, string_reg, sym_reg, console;
  
  hvm_gen_goto_label(gen->block, "defs");

  hvm_gen_sub(gen->block, "_js_new_object");
  obj = hvm_vm_reg_gen(0);
  hvm_gen_structnew(gen->block,  obj);
  hvm_gen_return(gen->block, obj);

  hvm_gen_sub(gen->block, "_js_new_function");
  func    = hvm_vm_reg_gen(0);
  sym     = hvm_vm_reg_gen(1);
  arg_sym = hvm_vm_reg_param(0);
  hvm_gen_set_symbol(gen->block, sym, "_js_new_object");
  hvm_gen_callsymbolic(gen->block, sym, func); // Func will be object struct
  // Now set the internal symbol
  hvm_gen_set_symbol(gen->block, sym, "_Js_symbol");
  hvm_gen_structset(gen->block, func, sym, arg_sym);
  hvm_gen_return(gen->block, func);

  // Log function
  hvm_gen_sub(gen->block, "console.log");
  string_reg = hvm_vm_reg_param(0);
  sym_reg    = hvm_vm_reg_gen(0);
  // Copy string parameter into the argument
  hvm_gen_move(gen->block, hvm_vm_reg_arg(0), string_reg);
  hvm_gen_set_symbol(gen->block, sym_reg, "print");
  hvm_gen_callprimitive(gen->block, sym_reg, hvm_vm_reg_null());
  hvm_gen_return(gen->block, hvm_vm_reg_null());

  // Building the console object
  hvm_gen_label(gen->block, "defs");
  // Creating the function
  func = hvm_vm_reg_gen(0);
  sym  = hvm_vm_reg_gen(1);
  hvm_gen_set_symbol(gen->block, sym, "_js_new_function");
  hvm_gen_set_symbol(gen->block, hvm_vm_reg_arg(0), "console.log");
  hvm_gen_callsymbolic(gen->block, sym, func);
  // Adding the function to the console object
  console = hvm_vm_reg_gen(2);
  hvm_gen_set_symbol(gen->block, sym, "_js_new_object");
  hvm_gen_callsymbolic(gen->block, sym, console); // console now an object
  // Add function object in func to console
  hvm_gen_set_symbol(gen->block, sym, "log");
  hvm_gen_structset(gen->block, console, sym, func);
  // Add console to the locals and globals
  hvm_gen_set_symbol(gen->block, sym, "console");
  hvm_gen_setlocal(gen->block, sym, console);
  hvm_gen_setglobal(gen->block, sym, console);
  
  hvm_gen_die(gen->block);

  hvm_chunk *chunk = hvm_gen_chunk(gen);
  hvm_chunk_disassemble(chunk);

  hvm_vm *vm = hvm_new_vm();
  hvm_bootstrap_primitives(vm);

  printf("LOADING...\n");
  hvm_vm_load_chunk(vm, chunk);

  printf("AFTER LOADING:\n");
  hvm_print_data(vm->program, vm->program_size);

  printf("RUNNING...\n");
  hvm_vm_run(vm);

  return;
  // printf("\nAFTER RUNNING:\n");
  // hvm_obj_ref *reg = vm->general_regs[hvm_vm_reg_gen(2)];
  // printf("$2->type = %d\n", reg->type);
  // assert(reg->type == HVM_INTEGER);
  // printf("$2->value = %lld\n", reg->data.i64);
}



int main(int argc, char **argv) {
  test_generator();
  // test_heap();

  /*
  hvm_vm *vm = hvm_new_vm();

  hvm_obj_string *str = hvm_new_obj_string();
  str->data = "test";
  hvm_obj_ref *ref = hvm_new_obj_ref();
  hvm_obj_ref_set_string(ref, str);
  hvm_vm_set_const(vm, 1234567, ref);

  vm->program[0] = HVM_OP_NOOP;
  vm->program[1] = HVM_OP_SETSTRING;
  vm->program[2] = 1;// register destination
  *(uint32_t*)&vm->program[3] = 1234567;// 32-bit integer const index

  vm->program[7] = HVM_OP_NOOP;
  vm->program[8] = HVM_OP_DIE;

  hvm_vm_run(vm);

  hvm_obj_ref *reg;
  reg = vm->general_regs[1];
  printf("reg: %p\n", reg);
  printf("reg->type: %d\n", reg->type);
  hvm_obj_string *str2;
  str2 = (hvm_obj_string*)(reg->data);
  printf("str2->data: %s\n", str2->data);
  */

  /*
  hvm_symbol_table *st = new_hvm_symbol_table();
  printf("size = %llu\n", st->size);
  uint64_t a, b, c, a2;
  a = hvm_symbolicate(st, "a");
  b = hvm_symbolicate(st, "b");
  c = hvm_symbolicate(st, "c");
  a2 = hvm_symbolicate(st, "a");
  printf("a:  %llu\n", a);
  printf("b:  %llu\n", b);
  printf("c:  %llu\n", c);
  printf("a2: %llu\n", a2);
  printf("size = %llu\n", st->size);
  */
  return 0;
}
