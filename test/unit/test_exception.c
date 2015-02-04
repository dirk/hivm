#include "preamble.h"

int main(int argc, char const *argv[]) {
  hvm_gen *gen = hvm_new_gen();
  hvm_obj_ref *obj0 = NULL;
  hvm_obj_ref *obj1 = NULL;

  byte reg0 = hvm_vm_reg_gen(0);
  byte reg1 = hvm_vm_reg_gen(1);

  // Immediately jump to the code that's going to throw the exception
  hvm_gen_goto_label(gen->block, "main");

  // Label for the code to actually catch the exception
  hvm_gen_label(gen->block, "catch");
  // Do a straight die so the test harness can check the register and make
  // sure there's an exception object in it
  hvm_gen_die(gen->block);

  hvm_gen_label(gen->block, "main");
  // Register our exception handler for the frame
  hvm_gen_catch_label(gen->block, "catch", reg0);
  hvm_gen_structnew(gen->block, reg1);
  hvm_gen_throw(gen->block, reg1);
  hvm_gen_die(gen->block);

  hvm_vm *vm = gen_chunk_and_run(gen);

  obj0 = vm->general_regs[0];
  obj1 = vm->general_regs[1];
  // Make sure the object thrown and the object in the catch register are
  // the same
  assert_true(obj0 == obj1, "Expected thrown object and caught object to be the same");

  return done();
}
