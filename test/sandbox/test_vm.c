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

void test_loop(hvm_gen *gen) {
  char i = hvm_vm_reg_gen(1), n = hvm_vm_reg_gen(2), incr = hvm_vm_reg_gen(3), z = hvm_vm_reg_gen(0);

  hvm_gen_litinteger(gen->block, i, 1);
  hvm_gen_litinteger(gen->block, n, 10);
  hvm_gen_litinteger(gen->block, incr, 1);// Incrementing by one
  hvm_gen_label(gen->block, "loop");
  hvm_gen_lt(gen->block, z, n, i); // 0 = i < n
  hvm_gen_if_label(gen->block, z, "end");
  hvm_gen_move(gen->block, hvm_vm_reg_arg(0), i);
  hvm_gen_set_symbol(gen->block, z, "int_to_string");
  hvm_gen_callprimitive(gen->block, z, z);
  hvm_gen_move(gen->block, hvm_vm_reg_arg(0), z);
  hvm_gen_set_symbol(gen->block, z, "print");
  hvm_gen_callprimitive(gen->block, z, hvm_vm_reg_null());
  hvm_gen_set_string(gen->block, hvm_vm_reg_arg(0), "\n");
  hvm_gen_callprimitive(gen->block, z, hvm_vm_reg_null());
  hvm_gen_add(gen->block, i, i, incr);
  hvm_gen_goto_label(gen->block, "loop");
  hvm_gen_label(gen->block, "end");
  hvm_gen_die(gen->block);
}

void test_exception_catch(hvm_gen *gen) {
  char z = hvm_vm_reg_gen(0), e = hvm_vm_reg_gen(1);

  hvm_gen_catch_label(gen->block, "catch", e);
  hvm_gen_set_symbol(gen->block, hvm_vm_reg_gen(2), "missing");
  hvm_gen_getlocal(gen->block, hvm_vm_reg_gen(1), hvm_vm_reg_gen(2));
  hvm_gen_die(gen->block);
  // Exception handler
  hvm_gen_label(gen->block, "catch");
  hvm_gen_set_string(gen->block, hvm_vm_reg_arg(0), "Caught exception!\n");
  hvm_gen_set_symbol(gen->block, z, "print");
  hvm_gen_callprimitive(gen->block, z, hvm_vm_reg_null());
  hvm_gen_set_symbol(gen->block, z, "print_exception");
  hvm_gen_move(gen->block, hvm_vm_reg_arg(0), e);
  hvm_gen_callprimitive(gen->block, z, hvm_vm_reg_null());
  hvm_gen_clearcatch(gen->block);
  hvm_gen_die(gen->block);
}

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

  // test_loop(gen);
  test_exception_catch(gen);

  /*
  // Hijinks-like code
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
  hvm_gen_set_symbol(gen->block, sym, "_js_symbol");
  hvm_gen_structset(gen->block, func, sym, arg_sym);
  hvm_gen_return(gen->block, func);

  // Log function
  hvm_gen_sub(gen->block, "console.log");
  hvm_gen_set_debug_entry(gen->block, 0, "console.log");
  string_reg = hvm_vm_reg_param(0);
  sym_reg    = hvm_vm_reg_gen(0);
  // Copy string parameter into the argument
  hvm_gen_move(gen->block, hvm_vm_reg_arg(0), string_reg);
  hvm_gen_set_symbol(gen->block, sym_reg, "print");
  hvm_gen_callprimitive(gen->block, sym_reg, hvm_vm_reg_null());
  hvm_gen_return(gen->block, hvm_vm_reg_null());

  // Building the console object
  hvm_gen_label(gen->block, "defs");
  hvm_gen_set_debug_entry(gen->block, 0, "(main)");
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
  */

  /*
  hvm_gen_set_integer(gen->block, 0, 1);
  hvm_gen_set_string(gen->block, 1, "test");
  hvm_gen_add(gen->block, 2, 0, 1);
  */

  /*
  hvm_gen_goto_label(gen->block, "tail");
  hvm_gen_label(gen->block, "head");
  hvm_gen_set_symbol(gen->block, hvm_vm_reg_gen(0), "print");
  hvm_gen_set_string(gen->block, hvm_vm_reg_arg(0), "test\n");
  // TODO: Currently doesn't throw an exception if symbol not found
  // hvm_gen_callsymbolic(gen->block, 0, hvm_vm_reg_null());
  hvm_gen_callprimitive(gen->block, hvm_vm_reg_gen(0), hvm_vm_reg_null());
  hvm_gen_die(gen->block);
  hvm_gen_label(gen->block, "tail");
  hvm_gen_litinteger_label(gen->block, 1, "head");
  hvm_gen_gotoaddress(gen->block, 1);
  hvm_gen_die(gen->block);
  */

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
