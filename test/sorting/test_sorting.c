#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include <glib.h>

#include "hvm.h"
#include "hvm_symbol.h"
#include "hvm_object.h"
#include "hvm_chunk.h"
#include "hvm_generator.h"
#include "hvm_bootstrap.h"
#include "hvm_debug.h"

static const char *array = "array";

void build_array(hvm_gen *gen, unsigned int size) {
  srand(2014);
  // Array register
  byte ar = hvm_vm_reg_gen(1);
  byte r2 = hvm_vm_reg_gen(2);
  hvm_gen_litinteger(gen->block, r2, (int64_t)size);
  // Create array in $ar with size $r2.
  hvm_gen_arraynew(gen->block, ar, r2);

  // `rand` symbol to invoke the rand primitive for build the array
  byte sym = hvm_vm_reg_gen(0);
  hvm_gen_set_symbol(gen->block, sym, "rand");

  // Initial state for our loop
  byte idx       = hvm_vm_reg_gen(2);
  byte last_idx  = hvm_vm_reg_gen(3);
  byte r4        = hvm_vm_reg_gen(4);
  byte increment = hvm_vm_reg_gen(5);
  hvm_gen_litinteger(gen->block, increment, 1);
  hvm_gen_litinteger(gen->block, idx, 0);
  hvm_gen_litinteger(gen->block, last_idx, (int64_t)(size - 1));
  // Loop condition (A = B < C)
  hvm_gen_label(gen->block, "condition");
  hvm_gen_eq(gen->block, r4, idx, last_idx);// r4 = (idx == len)
  hvm_gen_if_label(gen->block, r4, "end");
  // Body of the loop
    // Set the array
    hvm_gen_invokeprimitive(gen->block, sym, r4);// rand() -> r4
    hvm_gen_arrayset(gen->block, ar, idx, r4);// ar[idx] = r4
    // Increment the index
    hvm_gen_add(gen->block, idx, idx, increment);
    hvm_gen_goto_label(gen->block, "condition");
  // End
  hvm_gen_label(gen->block, "end");
}

int main(int argc, char **argv) {
  static const unsigned int array_size = 1000;

  hvm_gen *gen = hvm_new_gen();
  hvm_gen_set_file(gen, "sorting");

  // Add the sequence to build the big array
  build_array(gen, array_size);

  hvm_chunk *chunk = hvm_gen_chunk(gen);
  hvm_chunk_disassemble(chunk);
  return 0;

  hvm_vm *vm = hvm_new_vm();
  hvm_bootstrap_primitives(vm);

  printf("LOADING...\n");
  hvm_vm_load_chunk(vm, chunk);

  printf("AFTER LOADING:\n");
  hvm_print_data(vm->program, vm->program_size);

  //hvm_debug_begin(vm);

  printf("RUNNING...\n");
  hvm_vm_run(vm);

  // printf("\nAFTER RUNNING:\n");
  // hvm_obj_ref *reg = vm->general_regs[hvm_vm_reg_gen(2)];
  // printf("$2->type = %d\n", reg->type);
  // assert(reg->type == HVM_INTEGER);
  // printf("$2->value = %lld\n", reg->data.i64);

  return 0;
}
