#include "../../deps/greatest.h"

#include <assert.h>

#include "hvm.h"
#include "hvm_symbol.h"
#include "hvm_object.h"
#include "hvm_chunk.h"
#include "hvm_generator.h"
#include "hvm_bootstrap.h"

hvm_vm *run_chunk(hvm_chunk *chunk) {
  hvm_vm *vm = hvm_new_vm();
  hvm_bootstrap_primitives(vm);
  hvm_vm_load_chunk(vm, chunk);
  hvm_vm_run(vm);
  return vm;
}

TEST int_comparison_test() {
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
  hvm_chunk *chunk = hvm_gen_chunk(gen);
  hvm_vm *vm = run_chunk(chunk);

  // Assertions
  obj = vm->general_regs[hvm_vm_reg_gen(lt_reg)];
  ASSERT_EQ(obj->data.i64, 1);
  obj = vm->general_regs[hvm_vm_reg_gen(gt_reg)];
  ASSERT_EQ(obj->data.i64, 1);
  obj = vm->general_regs[hvm_vm_reg_gen(lte_reg)];
  ASSERT_EQ(obj->data.i64, 1);
  obj = vm->general_regs[hvm_vm_reg_gen(gte_reg)];
  ASSERT_EQ(obj->data.i64, 1);

  PASS();
}

SUITE(test_suite) {
  RUN_TEST(int_comparison_test);
}

GREATEST_MAIN_DEFS();
int main(int argc, char *argv[]) {
  GREATEST_MAIN_BEGIN();
  RUN_SUITE(test_suite);
  GREATEST_MAIN_END();
  return 0;
}

