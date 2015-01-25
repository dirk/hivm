#include "preamble.h"

int main(int argc, char const *argv[]) {
  hvm_gen *gen = hvm_new_gen();

  int64_t val_callsymbolic = 1;
  int64_t val_call         = 2;

  byte reg0 = hvm_vm_reg_gen(0);
  byte reg1 = hvm_vm_reg_gen(1);
  byte reg2 = hvm_vm_reg_gen(2);
  byte reg3 = hvm_vm_reg_gen(3);
  byte reg4 = hvm_vm_reg_gen(4);
  hvm_obj_ref *obj;

  hvm_gen_goto_label(gen->block, "main");

  // For testing CALLSYMBOLIC (call using a symbol to be looked up in the
  // subroutine map)
  hvm_gen_sub(gen->block, "callsymbolic");
  hvm_gen_litinteger(gen->block, reg0, val_callsymbolic);
  hvm_gen_return(gen->block, reg0);

  // For testing CALL with a hard-coded (but also relocated address)
  hvm_gen_sub(gen->block, "call");
  hvm_gen_litinteger(gen->block, reg0, val_call);
  hvm_gen_return(gen->block, reg0);

  hvm_gen_label(gen->block, "main");
  hvm_gen_callsymbolic(gen->block, "callsymbolic", reg1);
  hvm_gen_call_label(gen->block, "call", reg2);
  // Let's also do a basic test of CALLPRIMITIVE by creating an array in reg3
  // and cloning it into reg4
  hvm_gen_arraynew(gen->block, reg3, hvm_vm_reg_null());
  hvm_gen_move(gen->block, hvm_vm_reg_arg(0), reg3);
  hvm_gen_callprimitive(gen->block, "array_clone", reg4);
  hvm_gen_die(gen->block);

  hvm_vm *vm = gen_chunk_and_run(gen);

  // Assertions
  obj = vm->general_regs[reg1];
  assert_true(obj->data.i64 == val_callsymbolic, "Expected CALLSYMBOLIC return to be 1");
  obj = vm->general_regs[reg2];
  assert_true(obj->data.i64 == val_call, "Expected CALL return to be 2");
  // Check that the array_clone'd array in reg4 is different from the one
  // in reg3
  hvm_obj_ref *arr3 = vm->general_regs[reg3];
  hvm_obj_ref *arr4 = vm->general_regs[reg4];
  assert_true(arr4->type == HVM_ARRAY, "Expected array_clone to return an array");
  assert_true(arr3->data.v != arr4->data.v, "Expected array_clone to return a new array");

  return done();
}
