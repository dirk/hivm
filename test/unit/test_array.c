#include "preamble.h"

int main(int argc, char const *argv[]) {
  hvm_gen *gen = hvm_new_gen();

  int64_t test_value = 42;

  byte reg0                = hvm_vm_reg_gen(0);
  byte reg_empty_array     = hvm_vm_reg_gen(1);
  byte reg_key             = hvm_vm_reg_gen(2);
  byte reg_value_to_set    = hvm_vm_reg_gen(3);
  byte reg_value_retrieved = hvm_vm_reg_gen(4);
  byte reg_scratch_array   = hvm_vm_reg_gen(5);
  byte reg_index           = hvm_vm_reg_gen(6);
  hvm_obj_ref *obj;

  // Creating an empty array works
  hvm_gen_arraynew(gen->block, reg_empty_array, hvm_vm_reg_null());

  // Creating a 1-item array for get-set tests
  hvm_gen_litinteger(gen->block, reg0, 1);
  hvm_gen_arraynew(gen->block, reg_scratch_array, reg0);
  // Set the value
  hvm_gen_litinteger(gen->block, reg_value_to_set, test_value);
  hvm_gen_litinteger(gen->block, reg_index, 0);
  hvm_gen_arrayset(gen->block, reg_scratch_array, reg_index, reg_value_to_set);
  // Get the value into the _retrieved register
  hvm_gen_arrayget(gen->block, reg_value_retrieved, reg_scratch_array, reg_index);

  hvm_gen_die(gen->block);

  hvm_vm *vm = gen_chunk_and_run(gen);

  return done();
}
