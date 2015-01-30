#include "preamble.h"

int main(int argc, char const *argv[]) {
  hvm_gen *gen = hvm_new_gen();

  int64_t val_start = 0;
  int64_t val_iterations = 5;
  int64_t incr = 2;

  byte reg0    = hvm_vm_reg_gen(0);
  byte reg_ctr = hvm_vm_reg_gen(1);
  byte reg_max = hvm_vm_reg_gen(2);
  byte reg_acc = hvm_vm_reg_gen(3);
  hvm_obj_ref *obj;

  // Set up the starting counter, the maximum number of iterations, and
  // the accumulator
  hvm_gen_litinteger(gen->block, reg_ctr, val_start);
  hvm_gen_litinteger(gen->block, reg_max, val_iterations);
  hvm_gen_litinteger(gen->block, reg_acc, 0);

  hvm_gen_label(gen->block, "condition");
  hvm_gen_eq(gen->block, reg0, reg_ctr, reg_max);
  hvm_gen_if_label(gen->block, reg0, "end");

  // Do the summation and increment the counter
  hvm_gen_litinteger(gen->block, reg0, incr);
  hvm_gen_add(gen->block, reg_acc, reg_acc, reg0);
  hvm_gen_litinteger(gen->block, reg0, 1);
  hvm_gen_add(gen->block, reg_ctr, reg_ctr, reg0);
  hvm_gen_goto_label(gen->block, "condition");

  hvm_gen_label(gen->block, "end");
  hvm_gen_die(gen->block);

  hvm_vm *vm = gen_chunk_and_run(gen);

  // Assertions
  obj = vm->general_regs[reg_ctr];
  assert_true(obj->data.i64 == val_iterations, "Expected counter to equal number of iterations");

  obj = vm->general_regs[reg_acc];
  assert_true(obj->data.i64 == (val_iterations * incr), "Expected accumulator to be correct value");

  return done();
}
