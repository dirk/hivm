
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>

#include <llvm-c/Core.h>

#include "vm.h"
#include "object.h"
#include "jit-tracer.h"
#include "jit-compiler.h"

static LLVMContextRef hvm_shared_llvm_context;
static LLVMModuleRef  hvm_shared_llvm_module;

LLVMContextRef hvm_get_llvm_context() {
  if(hvm_shared_llvm_context == NULL) {
    // Set up our context and module
    hvm_shared_llvm_context = LLVMContextCreate();
    hvm_shared_llvm_module  = LLVMModuleCreateWithNameInContext("hvm", hvm_shared_llvm_context);
  }
  return hvm_shared_llvm_context;
}
LLVMModuleRef hvm_get_llvm_module() {
  assert(hvm_shared_llvm_module != NULL);
  return hvm_shared_llvm_module;
}

void hvm_jit_compile_resolve_registers(hvm_call_trace *trace, hvm_compile_bundle *bundle) {
  hvm_compile_sequence_data *data = bundle->data;
  for(unsigned int i = 0; i < trace->sequence_length; i++) {
    hvm_compile_sequence_data *item = &data[i];
    // Do stuff with the item based upon its type.
  }
}

void hvm_jit_compile_trace(hvm_call_trace *trace) {
  LLVMContextRef context = hvm_get_llvm_context();
  LLVMModuleRef  module = hvm_get_llvm_module();
  // Builder that we'll write the instructions from our trace into
  LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);

  // Allocate an array of data items for each item in the trace
  hvm_compile_sequence_data *data = malloc(sizeof(hvm_compile_sequence_data) * trace->sequence_length);
  // Establish a bundle for all of our stuff related to this compilation.
  hvm_compile_bundle bundle = {
    .data = data
  };

  // Eventually going to run this as a hopefully-two-pass compilation. For now
  // though it's going to be multi-pass.

  // Resolve register references in instructions into concrete IR value
  // references.
  hvm_jit_compile_resolve_registers(trace, &bundle);

  // Identify and extract gets/sets of globals and locals into dedicated
  // in-out pointers arguments to the block so that they can be passed
  // by the VM into the block at call time.

  // Identify potential guard points to be checked before/during/after
  // execution.

  // Use all of the above and the trace itself to generate code and compile to
  // an optimized native representation.

  free(data);
}
