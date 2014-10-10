
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>

#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>

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

LLVMTypeRef hvm_jit_get_llvm_pointer_type() {
  static LLVMTypeRef pointer_type;
  if(!pointer_type) {
    // Generic pointer type
    pointer_type = LLVMPointerType(LLVMVoidType(), 0);
  }
  return pointer_type;
}

LLVMValueRef hvm_jit_get_obj_array_get_llvm_value(hvm_compile_bundle *bundle) {
  static LLVMValueRef func;
  if(!func) {
    LLVMModuleRef module          = bundle->llvm_module;
    LLVMExecutionEngineRef engine = bundle->llvm_engine;
    LLVMTypeRef ptr_type          = hvm_jit_get_llvm_pointer_type();
    // Last argument tells LLVM it's non-variadic
    LLVMTypeRef return_type    = ptr_type;
    LLVMTypeRef param_types[2] = {ptr_type, ptr_type};
    LLVMTypeRef func_type      = LLVMFunctionType(return_type, param_types, 2, false);
    // LLVMAddFunction calls the following LLVM C++:
    //   Function::Create(functiontype, GlobalValue::ExternalLinkage, name, module)
    LLVMValueRef func = LLVMAddFunction(module, "hvm_obj_array_get", func_type);
    // Then register our function pointer as a global external linkage in
    // the execution engine.
    LLVMAddGlobalMapping(engine, func, &hvm_obj_array_get);
  }
  return func;
}

#define JIT_SAVE_DATA_ITEM_AND_VALUE(REG, DATA_ITEM, VALUE) \
  if(REG <= 127) { \
    general_reg_data_sources[REG] = DATA_ITEM; \
    general_reg_values[REG]       = VALUE; \
  } else { \
    fprintf(stderr, "jit-compiler: Cannot handle write to register type %d\n", REG); \
    assert(false); \
  }

void hvm_jit_compile_resolve_registers(hvm_vm *vm, hvm_call_trace *trace, hvm_compile_bundle *bundle) {
  unsigned int i;
  // Sequence data items for the instruction that set a given register.
  hvm_compile_sequence_data *general_reg_data_sources[HVM_GENERAL_REGISTERS];
  // Actual values in a register
  LLVMValueRef general_reg_values[HVM_GENERAL_REGISTERS];
  // Initialize our pseudo-registers to NULL
  for(i = 0; i < HVM_GENERAL_REGISTERS; i++) {
    general_reg_data_sources[i] = NULL;
    general_reg_values[i]       = NULL;
  }

  byte reg;
  unsigned int type;
  hvm_compile_sequence_data *data = bundle->data;

  // LLVMModuleRef  module  = bundle->llvm_module;
  LLVMBuilderRef builder = bundle->llvm_builder;

  // Set up our generic pointer type (in the 0 address space)
  LLVMTypeRef pointer_type = hvm_jit_get_llvm_pointer_type();
  // LLVMTypeRef int64_type   = LLVMInt64Type();

  for(i = 0; i < trace->sequence_length; i++) {
    hvm_compile_sequence_data *data_item  = &data[i];
    hvm_trace_sequence_item   *trace_item = &trace->sequence[i];
    // Do stuff with the item based upon its type.
    switch(trace_item->head.type) {
      case HVM_TRACE_SEQUENCE_ITEM_SETSYMBOL:
        data_item->setsymbol.type = HVM_COMPILE_DATA_SETSYMBOL;
        reg = trace_item->setsymbol.reg;
        // Copy over our basic information from the trace item
        data_item->setsymbol.reg      = reg;
        data_item->setsymbol.constant = trace_item->setsymbol.constant;
        // Also compile our symbol as a LLVM value
        hvm_obj_ref *ref = hvm_const_pool_get_const(&vm->const_pool, data_item->setsymbol.constant);
        // Integer constant wants an `unsigned long long`.
        LLVMValueRef value = LLVMConstInt(pointer_type, (unsigned long long)ref, true);
        // Save our new value into the data item.
        data_item->setsymbol.value = value;
        // TODO: Call a VM function to check this
        JIT_SAVE_DATA_ITEM_AND_VALUE(reg, data_item, value);
        break;

      case HVM_TRACE_SEQUENCE_ITEM_ARRAYGET:
        data_item->arrayget.type = HVM_COMPILE_DATA_ARRAYGET;
        // Getting the pointer value to the array
        byte reg_array = trace_item->arrayget.register_array;
        assert(general_reg_values[reg_array] != NULL);
        LLVMValueRef array = general_reg_values[reg_array];
        // Getting the index value
        byte reg_index = trace_item->arrayget.register_index;
        assert(general_reg_values[reg_index] != NULL);
        LLVMValueRef index = general_reg_values[reg_index];
        // Get the function as a LLVM value we can work with
        LLVMValueRef func = hvm_jit_get_obj_array_get_llvm_value(bundle);
        LLVMValueRef args[2] = {array, index};
        // Build the function call
        LLVMValueRef return_value = LLVMBuildCall(builder, func, args, 2, "arrayget");
        // Save the return value
        byte reg = trace_item->arrayget.register_value;
        JIT_SAVE_DATA_ITEM_AND_VALUE(reg, data_item, return_value);
        break;

      default:
        type = trace_item->head.type;
        fprintf(stderr, "jit-compiler: Don't know what to do with item type %d\n", type);
        break;
    }
  }
}

void hvm_jit_compile_trace(hvm_vm *vm, hvm_call_trace *trace) {
  LLVMContextRef context = hvm_get_llvm_context();
  LLVMModuleRef  module = hvm_get_llvm_module();
  // Builder that we'll write the instructions from our trace into
  LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);

  // Allocate an array of data items for each item in the trace
  hvm_compile_sequence_data *data = malloc(sizeof(hvm_compile_sequence_data) * trace->sequence_length);
  // Establish a bundle for all of our stuff related to this compilation.
  hvm_compile_bundle bundle = {
    .data = data,
    .llvm_module = module,
    .llvm_builder = builder
  };

  // Eventually going to run this as a hopefully-two-pass compilation. For now
  // though it's going to be multi-pass.

  // Resolve register references in instructions into concrete IR value
  // references.
  hvm_jit_compile_resolve_registers(vm, trace, &bundle);

  // Identify and extract gets/sets of globals and locals into dedicated
  // in-out pointers arguments to the block so that they can be passed
  // by the VM into the block at call time.

  // Identify potential guard points to be checked before/during/after
  // execution.

  // Use all of the above and the trace itself to generate code and compile to
  // an optimized native representation.

  free(data);
}
