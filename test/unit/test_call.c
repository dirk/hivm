#include "preamble.h"

int main(int argc, char const *argv[]) {
  hvm_gen *gen = hvm_new_gen();

  int64_t val_callsymbolic = 1;
  int64_t val_call         = 2;

  byte reg0 = hvm_vm_reg_gen(0);
  byte reg1 = hvm_vm_reg_gen(1);
  byte reg2 = hvm_vm_reg_gen(2);
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
  hvm_gen_die(gen->block);

  hvm_vm *vm = gen_chunk_and_run(gen);

  // Assertions
  obj = vm->general_regs[reg1];
  assert_true(obj->data.i64 == val_callsymbolic, "Expected CALLSYMBOLIC return to be 1");
  obj = vm->general_regs[reg2];
  assert_true(obj->data.i64 == val_call, "Expected CALL return to be 2");

  return done();
}
