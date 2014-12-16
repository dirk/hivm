#include "preamble.h"

int main(int argc, char const *argv[]) {
  hvm_gen *gen = hvm_new_gen();
  byte sym   = hvm_vm_reg_gen(0);
  byte strct = hvm_vm_reg_gen(1);
  byte reg2  = hvm_vm_reg_gen(2);
  byte reg3  = hvm_vm_reg_gen(3);
  hvm_obj_ref *obj;

  // Integer constant
  hvm_gen_litinteger(gen->block, reg2, 1);
  
  // Create and set on structure
  hvm_gen_structnew(gen->block, strct);
  hvm_gen_set_symbol(gen->block, sym, "key");
  hvm_gen_structset(gen->block, strct, sym, reg2);
  // Get from the structure into reg3
  hvm_gen_structget(gen->block, reg3, strct, sym);

  hvm_gen_die(gen->block);

  hvm_vm *vm = gen_chunk_and_run(gen);

  // Check that integer in $reg3 is expected
  obj = vm->general_regs[reg3];
  assert_true(obj->type == HVM_INTEGER, "General register 3 should be integer");
  assert_true(obj->data.i64 == 1, "General register 3 should be 1");
  // Check that there's a structure in $strct
  obj = vm->general_regs[strct];
  assert_true(obj->type == HVM_STRUCTURE, "Expected structure");

  return done();
}
