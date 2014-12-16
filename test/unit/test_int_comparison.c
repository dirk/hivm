#include "preamble.h"

int main(int argc, char const *argv[]) {
  hvm_gen *gen = hvm_new_gen();
  char lt_reg = 0, gt_reg = 1, lte_reg = 2, gte_reg = 3;
  char b = 10, c = 11;
  hvm_obj_ref *obj;

  hvm_gen_litinteger(gen->block, hvm_vm_reg_gen(b), 1);
  hvm_gen_litinteger(gen->block, hvm_vm_reg_gen(c), 2);
  // Less than
  hvm_gen_lt(gen->block, hvm_vm_reg_gen(lt_reg), hvm_vm_reg_gen(b), hvm_vm_reg_gen(c));
  // Greater than
  hvm_gen_gt(gen->block, hvm_vm_reg_gen(gt_reg), hvm_vm_reg_gen(c), hvm_vm_reg_gen(b));
  // Less than or equal
  hvm_gen_lte(gen->block, hvm_vm_reg_gen(lte_reg), hvm_vm_reg_gen(b), hvm_vm_reg_gen(c));
  // Greater than or equal
  hvm_gen_gte(gen->block, hvm_vm_reg_gen(gte_reg), hvm_vm_reg_gen(c), hvm_vm_reg_gen(b));
  hvm_gen_die(gen->block);

  hvm_vm *vm = gen_chunk_and_run(gen);

  // Assertions
  obj = vm->general_regs[hvm_vm_reg_gen(lt_reg)];
  assert_true(obj->data.i64 == 1, "Less-than test register should be 1");
  obj = vm->general_regs[hvm_vm_reg_gen(gt_reg)];
  assert_true(obj->data.i64 == 1, "Greater-than test register should be 1");
  obj = vm->general_regs[hvm_vm_reg_gen(lte_reg)];
  assert_true(obj->data.i64 == 1, "Less-than-or-equal test register should be 1");
  obj = vm->general_regs[hvm_vm_reg_gen(gte_reg)];
  assert_true(obj->data.i64 == 1, "Greater-than-or-equal test register should be 1");

  return done();
}
