#include <stdlib.h>
#include <stdbool.h>
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

static char *array = "array";

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
  hvm_gen_litinteger(gen->block, last_idx, (int64_t)size);
  // Loop condition (A = B < C)
  hvm_gen_label(gen->block, "build_array_condition");
  hvm_gen_eq(gen->block, r4, idx, last_idx);// $r4 = ($idx == $len)
  hvm_gen_if_label(gen->block, r4, "build_array_end");
  // Body of the loop
    // Set the array
    hvm_gen_invokeprimitive(gen->block, sym, r4);// rand() -> $r4
    // hvm_gen_arrayset(gen->block, ar, idx, r4);// $ar[$idx] = $r4
    hvm_gen_arraypush(gen->block, ar, r4);// $ar << $r4
    // Increment the index
    hvm_gen_add(gen->block, idx, idx, increment);
    hvm_gen_goto_label(gen->block, "build_array_condition");
  // End
  hvm_gen_label(gen->block, "build_array_end");
  hvm_gen_set_symbol(gen->block, sym, array);
  // local:array = $ar
  hvm_gen_setlocal(gen->block, sym, ar);
}

void define_insertion_sort(hvm_gen *gen) {
  hvm_gen_sub(gen->block, "insertion_sort");

  byte sym     = hvm_vm_reg_gen(0);
  byte i       = hvm_vm_reg_gen(1);
  byte j       = hvm_vm_reg_gen(2);
  byte x       = hvm_vm_reg_gen(3);
  byte len     = hvm_vm_reg_gen(10);
  byte r4      = hvm_vm_reg_gen(4);
  byte r5      = hvm_vm_reg_gen(5);
  byte arr     = hvm_vm_reg_gen(6);
  byte scratch = hvm_vm_reg_gen(7);

  byte a_jminus1 = hvm_vm_reg_gen(11);

  byte zero = hvm_vm_reg_gen(20);
  hvm_gen_litinteger(gen->block, zero, 0);
  byte neg1 = hvm_vm_reg_gen(21);
  hvm_gen_litinteger(gen->block, neg1, -1);
  byte one  = hvm_vm_reg_gen(22);
  hvm_gen_litinteger(gen->block, one, 1);

  // byte i_as_string = hvm_vm_reg_gen(23);

  // hvm_gen_move(gen->block, arr, hvm_vm_reg_param(0));
  // Esoteric get/setlocal testing
  // Put the param in local:array
  hvm_gen_move(gen->block, scratch, hvm_vm_reg_param(0));
  hvm_gen_set_symbol(gen->block, sym, "array");
  hvm_gen_setlocal(gen->block, sym, scratch);
  // Then fetch it out into $arr
  hvm_gen_getlocal(gen->block, arr, sym);

  // $i = 1
  hvm_gen_litinteger(gen->block, i, 1);
  // $len = arraylen($arr)
  hvm_gen_arraylen(gen->block, len, arr);
  //hvm_gen_add(gen->block, len, len, neg1);
  hvm_gen_label(gen->block, "insertion_sort_condition");
  // $r4 = ($i == $len)
  hvm_gen_eq(gen->block, r4, i, len);
  hvm_gen_if_label(gen->block, r4, "insertion_sort_end");
  // Loop body
    // $x = $arr[$i]
    hvm_gen_arrayget(gen->block, x, arr, i);

    // Printing $i as string
    // hvm_gen_set_symbol(gen->block, sym, "int_to_string");
    // hvm_gen_move(gen->block, hvm_vm_reg_arg(0), i);
    // hvm_gen_invokeprimitive(gen->block, sym, i_as_string);
    // hvm_gen_move(gen->block, hvm_vm_reg_arg(0), i_as_string);
    // hvm_gen_set_symbol(gen->block, sym, "print");
    // hvm_gen_invokeprimitive(gen->block, sym, hvm_vm_reg_null());
    // hvm_gen_set_string(gen->block, hvm_vm_reg_arg(0), "\n");
    // hvm_gen_invokeprimitive(gen->block, sym, hvm_vm_reg_null());

    // $j = $i
    hvm_gen_move(gen->block, j, i);
    // while $j > 0 and A[j-1] > A[j]
      hvm_gen_label(gen->block, "insertion_sort_inner_condition");
      // $r4 = $j > 0
      hvm_gen_gt(gen->block, r4, j, zero);
      // Checking the left side of the AND
        hvm_gen_eq(gen->block, r5, r4, zero);
        hvm_gen_if_label(gen->block, r5, "insertion_sort_inner_end");
      // $a_jminus1 = $arr[j - 1]
      hvm_gen_add(gen->block, r5, j, neg1);
      hvm_gen_arrayget(gen->block, a_jminus1, arr, r5);
      // $r5 = $a_jminus1 > $x
      hvm_gen_gt(gen->block, r5, a_jminus1, x);
      // $r4 = $r4 AND $r5
      hvm_gen_and(gen->block, r4, r4, r5);
      hvm_gen_if_label(gen->block, r4, "insertion_sort_inner_body");
      hvm_gen_goto_label(gen->block, "insertion_sort_inner_end");
      // Inner while body
        hvm_gen_label(gen->block, "insertion_sort_inner_body");
        // $arr[$j] = $a_jminus1
        hvm_gen_arrayset(gen->block, arr, j, a_jminus1);
        // $j = $j - 1
        hvm_gen_add(gen->block, j, j, neg1);
      hvm_gen_goto_label(gen->block, "insertion_sort_inner_condition");
    // Left inner while
    hvm_gen_label(gen->block, "insertion_sort_inner_end");
    // $arr[$j] = $x
    hvm_gen_arrayset(gen->block, arr, j, x);
    // $i = $i + 1
    hvm_gen_add(gen->block, i, i, one);
    hvm_gen_goto_label(gen->block, "insertion_sort_condition");
  // Left for loop
  hvm_gen_label(gen->block, "insertion_sort_end");

  // Print our current trace for debugging
  // hvm_gen_set_symbol(gen->block, sym, "debug_print_current_frame_trace");
  // hvm_gen_invokeprimitive(gen->block, sym, hvm_vm_reg_null());

  // Return the array
  hvm_gen_return(gen->block, arr);
}

int main(int argc, char **argv) {
  static const unsigned int array_size = 1000;

  hvm_gen *gen = hvm_new_gen();
  hvm_gen_set_file(gen, "sorting");

  // Add the sequence to build the big array
  build_array(gen, array_size);
  hvm_gen_goto_label(gen->block, "program");
  // Then insert our insertion sort subroutine
  define_insertion_sort(gen);

  // Main program
  hvm_gen_label(gen->block, "program");
  byte sym           = hvm_vm_reg_gen(0);
  byte ret           = hvm_vm_reg_gen(1);
  byte local_array   = hvm_vm_reg_gen(2);
  byte array_copy    = hvm_vm_reg_gen(3);
  byte timing        = hvm_vm_reg_gen(4);
  byte timings_array = hvm_vm_reg_gen(104);

  hvm_gen_arraynew(gen->block, timings_array, hvm_vm_reg_null());
  // hvm_gen_set_symbol(gen->block, sym, "timings");
  // hvm_gen_setlocal(gen->block, sym, timings_array);

  // Do the loop to invoke insertion sort a few times
  byte idx  = hvm_vm_reg_gen(100);
  byte lim  = hvm_vm_reg_gen(101);
  byte cond = hvm_vm_reg_gen(102);
  byte i    = hvm_vm_reg_gen(103);
  hvm_gen_litinteger(gen->block, idx, 0);
  hvm_gen_litinteger(gen->block, lim, 3);
  hvm_gen_label(gen->block, "loop_condition");
  // $cond = $idx == $lim
  hvm_gen_eq(gen->block, cond, idx, lim);
  hvm_gen_if_label(gen->block, cond, "loop_end");
    // Body of the loop
    // $local_array = local:array
    hvm_gen_set_symbol(gen->block, sym, array);
    hvm_gen_getlocal(gen->block, local_array, sym);
    // arg:0 = $local_array
    hvm_gen_move(gen->block, hvm_vm_reg_arg(0), local_array);
    // $array_copy = prim:array_clone($local_array)
    hvm_gen_set_symbol(gen->block, sym, "array_clone");
    hvm_gen_invokeprimitive(gen->block, sym, array_copy);
    // arraypush $timings_array (time_as_int())
    hvm_gen_set_symbol(gen->block, sym, "time_as_int");
    hvm_gen_invokeprimitive(gen->block, sym, timing);
    hvm_gen_arraypush(gen->block, timings_array, timing);
    // arg:0 = $array_copy
    hvm_gen_move(gen->block, hvm_vm_reg_arg(0), array_copy);
    // $ret = #insertion_sort()
    hvm_gen_callsymbolic_symbol(gen->block, "insertion_sort", ret);
    // arraypush $timings_array (time_as_int())
    hvm_gen_set_symbol(gen->block, sym, "time_as_int");
    hvm_gen_invokeprimitive(gen->block, sym, timing);
    hvm_gen_arraypush(gen->block, timings_array, timing);
    // $idx = $idx + 1
    hvm_gen_litinteger(gen->block, i, 1);
    hvm_gen_add(gen->block, idx, idx, i);
    hvm_gen_goto_label(gen->block, "loop_condition");
  // End of loop
  hvm_gen_label(gen->block, "loop_end");
  hvm_gen_die(gen->block);

  hvm_chunk *chunk = hvm_gen_chunk(gen);
  hvm_chunk_disassemble(chunk);
  //return 0;

  hvm_vm *vm = hvm_new_vm();
  hvm_bootstrap_primitives(vm);
  // Set the VM to *always* trace
  vm->always_trace = TRUE;

  printf("LOADING...\n");
  hvm_vm_load_chunk(vm, chunk);

  printf("AFTER LOADING:\n");
  hvm_print_data(vm->program, vm->program_size);

  //hvm_debug_begin(vm);

  // char buff[256];
  // printf("Press ENTER to continue...\n");
  // fgets(buff, 256, stdin);

  printf("RUNNING...\n");
  hvm_vm_run(vm);

  printf("\nDONE\n\n");

  // hvm_obj_ref *arrref = hvm_get_local(vm->top, hvm_symbolicate(vm->symbols, array));
  hvm_obj_ref *arrref = hvm_vm_register_read(vm, timings_array);
  assert(arrref->type == HVM_ARRAY);

  hvm_obj_array *arr = arrref->data.v;

  printf("timings_array = [%u]{\n", arr->array->len);
  for(unsigned int i = 0; i < arr->array->len; i++) {
    hvm_obj_ref *intval = g_array_index(arr->array, hvm_obj_ref*, i);
    assert(intval->type == HVM_INTEGER);
    printf("  %4u = %lld\n", i, intval->data.i64);
  }
  printf("}\n\n");

  printf("computed timings = {\n");
  for(unsigned int i = 0; i < arr->array->len; i += 2) {
    hvm_obj_ref *start = g_array_index(arr->array, hvm_obj_ref*, i);
    hvm_obj_ref *end   = g_array_index(arr->array, hvm_obj_ref*, i + 1);

    int64_t st = start->data.i64;
    int64_t et = end->data.i64;
    unsigned int run = ((i == 0 ? 1 : i) / 2) + 1;

    printf("  run[%d] = %lld microseconds\n", run, et - st);
  }
  printf("}\n");


  // printf("\nAFTER RUNNING:\n");
  // hvm_obj_ref *reg = vm->general_regs[hvm_vm_reg_gen(2)];
  // printf("$2->type = %d\n", reg->type);
  // assert(reg->type == HVM_INTEGER);
  // printf("$2->value = %lld\n", reg->data.i64);

  return 0;
}
