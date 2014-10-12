
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>

#include "vm.h"
#include "object.h"
#include "bootstrap.h"
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
LLVMTypeRef hvm_jit_get_llvm_void_type() {
  static LLVMTypeRef void_type;
  if(!void_type) {
    void_type = LLVMVoidType();
  }
  return void_type;
}

#define UNPACK_BUNDLE \
  LLVMModuleRef module          = bundle->llvm_module; \
  LLVMExecutionEngineRef engine = bundle->llvm_engine;

LLVMValueRef hvm_jit_obj_array_get_llvm_value(hvm_compile_bundle *bundle) {
  static LLVMValueRef func;
  if(!func) {
    UNPACK_BUNDLE;
    LLVMTypeRef ptr_type       = hvm_jit_get_llvm_pointer_type();
    LLVMTypeRef return_type    = ptr_type;
    LLVMTypeRef param_types[2] = {ptr_type, ptr_type};
    // Last argument tells LLVM it's non-variadic
    LLVMTypeRef func_type      = LLVMFunctionType(return_type, param_types, 2, false);
    // LLVMAddFunction calls the following LLVM C++:
    //   Function::Create(functiontype, GlobalValue::ExternalLinkage, name, module)
    func = LLVMAddFunction(module, "hvm_obj_array_get", func_type);
    // Then register our function pointer as a global external linkage in
    // the execution engine.
    LLVMAddGlobalMapping(engine, func, &hvm_obj_array_get);
  }
  return func;
}

LLVMValueRef hvm_jit_obj_array_set_llvm_value(hvm_compile_bundle *bundle) {
  static LLVMValueRef func;
  if(!func) {
    UNPACK_BUNDLE;
    LLVMTypeRef pointer_type = hvm_jit_get_llvm_pointer_type();
    LLVMTypeRef void_type    = hvm_jit_get_llvm_void_type();
    // Set up our parameters and return
    LLVMTypeRef return_type    = void_type;
    LLVMTypeRef param_types[3] = {pointer_type, pointer_type, pointer_type};
    LLVMTypeRef func_type      = LLVMFunctionType(return_type, param_types, 3, false);
    // Build the function and register it
    func = LLVMAddFunction(module, "hvm_obj_array_set", func_type);
    LLVMAddGlobalMapping(engine, func, &hvm_obj_array_set);
  }
  return func;
}

LLVMValueRef hvm_jit_vm_call_primitive_llvm_value(hvm_compile_bundle *bundle) {
  static LLVMValueRef func;
  if(!func) {
    UNPACK_BUNDLE;
    LLVMTypeRef pointer_type   = hvm_jit_get_llvm_pointer_type();
    LLVMTypeRef int64_type     = LLVMInt64Type();
    LLVMTypeRef param_types[2] = {pointer_type, int64_type};
    LLVMTypeRef func_type      = LLVMFunctionType(pointer_type, param_types, 2, false);
    // Build and register
    func = LLVMAddFunction(module, "hvm_vm_call_primitive", func_type);
    LLVMAddGlobalMapping(engine, func, &hvm_vm_call_primitive);
  }
  return func;
}

LLVMValueRef hvm_jit_obj_int_add_llvm_value(hvm_compile_bundle *bundle) {
  static LLVMValueRef func;
  if(!func) {
    UNPACK_BUNDLE;
    LLVMTypeRef pointer_type   = hvm_jit_get_llvm_pointer_type();
    LLVMTypeRef param_types[2] = {pointer_type, pointer_type};
    LLVMTypeRef func_type      = LLVMFunctionType(pointer_type, param_types, 2, false);
    // Build and register
    func = LLVMAddFunction(module, "hvm_obj_int_add", func_type);
    LLVMAddGlobalMapping(engine, func, &hvm_obj_int_add);
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

void hvm_jit_compile_builder(hvm_vm *vm, hvm_call_trace *trace, hvm_compile_bundle *bundle) {
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

  byte reg, reg_array, reg_index, reg_value, reg_symbol;
  unsigned int type;
  hvm_compile_sequence_data *data = bundle->data;
  LLVMValueRef value_array, value_index, value_symbol, value1, value2;
  // Function-pointer-as-value
  LLVMValueRef func;

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
        LLVMValueRef value = LLVMConstInt(pointer_type, (unsigned long long)ref, false);
        // Save our new value into the data item.
        data_item->setsymbol.value = value;
        // TODO: Call a VM function to check this
        JIT_SAVE_DATA_ITEM_AND_VALUE(reg, data_item, value);
        break;

      case HVM_TRACE_SEQUENCE_ITEM_ARRAYGET:
        data_item->arrayget.type = HVM_COMPILE_DATA_ARRAYGET;
        // Getting the pointer value to the array
        reg_array = trace_item->arrayget.register_array;
        assert(general_reg_values[reg_array] != NULL);
        LLVMValueRef array = general_reg_values[reg_array];
        // Getting the index value
        reg_index = trace_item->arrayget.register_index;
        assert(general_reg_values[reg_index] != NULL);
        LLVMValueRef index = general_reg_values[reg_index];
        // Get the function as a LLVM value we can work with
        func = hvm_jit_obj_array_get_llvm_value(bundle);
        LLVMValueRef arrayget_args[2] = {array, index};
        // Build the function call
        LLVMValueRef value_return = LLVMBuildCall(builder, func, arrayget_args, 2, "arrayget");
        // Save the return value
        byte reg = trace_item->arrayget.register_value;
        JIT_SAVE_DATA_ITEM_AND_VALUE(reg, data_item, value_return);
        break;

      case HVM_TRACE_SEQUENCE_ITEM_ARRAYSET:
        data_item->arrayset.type = HVM_COMPILE_DATA_ARRAYSET;
        // Get the values we need for the array-set operation
        reg_array = trace_item->arrayset.register_array;
        reg_index = trace_item->arrayset.register_index;
        reg_value = trace_item->arrayset.register_value;
        value_array = general_reg_values[reg_array];
        value_index = general_reg_values[reg_index];
        value       = general_reg_values[reg_value];
        // Get the array-set function
        func = hvm_jit_obj_array_set_llvm_value(bundle);
        LLVMValueRef arrayset_args[3] = {array, index, value};
        // Build the function call with the function value and arguments
        LLVMBuildCall(builder, func, arrayset_args, 3, "arrayset");
        break;

      case HVM_TRACE_SEQUENCE_ITEM_INVOKEPRIMITIVE:
        data_item->invokeprimitive.type = HVM_COMPILE_DATA_INVOKEPRIMITIVE;
        // Get the source value information
        reg_symbol = trace_item->invokeprimitive.register_symbol;
        value_symbol = general_reg_values[reg_symbol];
        assert(value_symbol != NULL);
        // Make a pointer to our VM
        LLVMValueRef value_vm = LLVMConstInt(pointer_type, (unsigned long long)vm, false);
        // Build the call to `hvm_vm_call_primitive`.
        func = hvm_jit_vm_call_primitive_llvm_value(bundle);
        LLVMValueRef invokeprimitive_args[2] = {value_vm, value_symbol};
        LLVMBuildCall(builder, func, invokeprimitive_args, 2, "invokeprimitive");
        break;

      case HVM_TRACE_SEQUENCE_ITEM_ADD:
        data_item->add.type = HVM_COMPILE_DATA_ADD;
        // Get the source values for the operation
        value1 = general_reg_values[trace_item->add.register_operand1];
        value2 = general_reg_values[trace_item->add.register_operand2];
        data_item->add.operand1 = value1;
        data_item->add.operand2 = value2;
        // Build our add operation
        func = hvm_jit_obj_int_add_llvm_value(bundle);
        LLVMValueRef add_args[2] = {value1, value2};
        LLVMBuildCall(builder, func, add_args, 2, "add");
        break;

      default:
        type = trace_item->head.type;
        fprintf(stderr, "jit-compiler: Don't know what to do with item type %d\n", type);
        break;
    }
  }
}

void hvm_jit_compile_insert_block(LLVMValueRef parent_func, hvm_jit_block *blocks, unsigned int *num_blocks_ptr, unsigned int index, uint64_t ip) {
  unsigned int num_blocks = *num_blocks_ptr;
  LLVMContextRef context = hvm_get_llvm_context();
  // Block to be operated upon
  hvm_jit_block *block;

  for(unsigned int i = 0; i < num_blocks; i++) {
    block = &blocks[i];
    // Don't duplicate blocks
    if(ip == block->ip) {
      return;
    }
    // If the given IP is greater than this block, then shift blocks
    // backwards and insert this block.
    if(ip > block->ip) {
      // Shuffle blocks backwards from the tail
      for(unsigned int n = num_blocks; n > i; n--) {
        struct hvm_jit_block *dest = &blocks[n];
        struct hvm_jit_block *src  = &blocks[n - 1];
        memcpy(dest, src, sizeof(struct hvm_jit_block));
      }
      // Insert the new block and return
      goto set_block;
    }
  }
  // Didn't find or insert the block, so tack it onto the tail
  block = &blocks[num_blocks];

set_block:
  block->ip          = ip;
  block->index       = index;
  block->basic_block = LLVMAppendBasicBlockInContext(context, parent_func, NULL);
  // Update the count and return
  *num_blocks_ptr = num_blocks + 1;
  return;
}

void hvm_jit_compile_identify_blocks(hvm_vm *vm, hvm_call_trace *trace, hvm_compile_bundle *bundle) {
  hvm_jit_block *blocks = malloc(sizeof(hvm_jit_block) * trace->sequence_length);
  unsigned int num_blocks;
  uint64_t ip;
  hvm_trace_sequence_item *item;

  // Unpack the bundle and get the context
  LLVMBuilderRef builder = bundle->llvm_builder;
  LLVMContextRef context = hvm_get_llvm_context();
  // Get the top-level basic block and its parent function
  LLVMBasicBlockRef insert_block = LLVMGetInsertBlock(builder);
  LLVMValueRef      parent_func  = LLVMGetBasicBlockParent(insert_block);

  // Set up a block for our entry point
  hvm_jit_block *entry = &blocks[0];
  // Get the first item in the trace (entry point)
  assert(trace->sequence_length > 0);
  item = &trace->sequence[0];
  entry->ip          = item->head.ip;
  entry->index       = 0;
  entry->basic_block = LLVMAppendBasicBlockInContext(context, parent_func, NULL);
  // Set num_blocks to reflect the existence of our entry function
  num_blocks = 1;

  for(unsigned int i = 0; i < trace->sequence_length; i++) {
    item = &trace->sequence[i];
    switch(item->head.type) {
      case HVM_OP_IF:
        ip = item->item_if.destination;
        hvm_jit_compile_insert_block(parent_func, blocks, &num_blocks, i, ip);
        break;
      case HVM_OP_GOTO:
        ip = item->item_goto.destination;
        hvm_jit_compile_insert_block(parent_func, blocks, &num_blocks, i, ip);
        break;
      default:
        continue;
    }
  }
  // Now let's trim down the blocks array to the size we actually need.
  blocks = realloc(blocks, sizeof(hvm_jit_block) * num_blocks);
  // And save them in the bundle
  bundle->blocks        = blocks;
  bundle->blocks_length = num_blocks;
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

  // Break our sequence into blocks based upon possible destinations of
  // jumps/ifs/gotos/etc.
  hvm_jit_compile_identify_blocks(vm, trace, &bundle);

  // Resolve register references in instructions into concrete IR value
  // references and build the instruction sequence.
  hvm_jit_compile_builder(vm, trace, &bundle);

  // Identify and extract gets/sets of globals and locals into dedicated
  // in-out pointers arguments to the block so that they can be passed
  // by the VM into the block at call time.

  // Identify potential guard points to be checked before/during/after
  // execution.

  // Use all of the above and the trace itself to generate code and compile to
  // an optimized native representation.

  free(data);
}
