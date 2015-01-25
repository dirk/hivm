#include "preamble.h"

int main(int argc, char const *argv[]) {
  hvm_gen *gen = hvm_new_gen();

  int64_t val = 1;
  byte reg0 = hvm_vm_reg_gen(0);
  byte reg1 = hvm_vm_reg_gen(1);
  hvm_obj_ref *obj0, *obj1;
  char *invoke_label = "invoke";
  char *main_label   = "main";

  hvm_gen_goto_label(gen->block, main_label);

  // Setup the invoke subroutine
  hvm_gen_sub(gen->block, invoke_label);
  hvm_gen_litinteger(gen->block, reg0, val);
  hvm_gen_return(gen->block, reg0);

  hvm_gen_label(gen->block, main_label);
  // Save the address for the invoke label in a register for use
  hvm_gen_litinteger_label(gen->block, reg1, invoke_label);
  // Call INVOKEADDRESS with result in reg0
  hvm_gen_invokeaddress(gen->block, reg1, reg0);
  hvm_gen_die(gen->block);

  hvm_vm *vm = gen_chunk_and_run(gen);

  // Assertions

  // Make sure that an okay-looking address value was set in reg1
  obj1 = vm->general_regs[reg1];
  int64_t addr = obj1->data.i64;
  assert_true(obj1->type == HVM_INTEGER, "Expected integer in register 1");
  assert_true(addr > 0 && addr < 32, "Expected reasonable value in register 1");

  // Make sure that the invocation returned the expected value (in reg0)
  obj0 = vm->general_regs[reg0];
  assert_true(obj0->type == HVM_INTEGER, "Expected integer in register 0");
  assert_true(obj0->data.i64 == val, "Expected register 0 to contain value 1");

  return done();
}
