#include "preamble.h"

int main(int argc, char const *argv[]) {
  hvm_gen *gen = hvm_new_gen();
  byte reg0 = hvm_vm_reg_gen(0);
  byte reg1 = hvm_vm_reg_gen(1);
  hvm_obj_ref *obj;

  hvm_gen_goto_label(gen->block, "main");

  hvm_gen_sub(gen->block, "call");
  hvm_gen_litinteger(gen->block, reg0, 1);
  hvm_gen_return(gen->block, reg0);

  hvm_gen_label(gen->block, "main");
  hvm_gen_callsymbolic(gen->block, "call", reg1);
  hvm_gen_die(gen->block);

  hvm_vm *vm = gen_chunk_and_run(gen);

  // Assertions
  obj = vm->general_regs[reg1];
  assert_true(obj->data.i64 == 1, "Expected return register to be 1");

  return done();
}
