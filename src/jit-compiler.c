
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

LLVMValueRef hvm_jit_obj_array_len_llvm_value(hvm_compile_bundle *bundle) {
  static LLVMValueRef func;
  if(!func) {
    UNPACK_BUNDLE;
    LLVMTypeRef pointer_type   = hvm_jit_get_llvm_pointer_type();
    LLVMTypeRef param_types[1] = {pointer_type};
    LLVMTypeRef func_type      = LLVMFunctionType(pointer_type, param_types, 1, false);
    // Build and register
    func = LLVMAddFunction(module, "hvm_obj_array_len", func_type);
    LLVMAddGlobalMapping(engine, func, &hvm_obj_array_len);
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

hvm_jit_block *hvm_jit_get_current_block(hvm_compile_bundle *bundle, unsigned int index) {
  // Track the previous block since that's what we'll actually be returning
  hvm_jit_block *prev = &bundle->blocks[0];

  for(unsigned int i = 0; i < bundle->blocks_length; i++) {
    hvm_jit_block *block = &bundle->blocks[i];
    // Return the previous block if we've run over
    if(block->index > index) {
      return prev;
    }
    prev = block;
  }
  return prev;
}

hvm_jit_block *hvm_jit_get_block_by_ip(hvm_compile_bundle *bundle, uint64_t ip) {
  for(unsigned int i = 0; i < bundle->blocks_length; i++) {
    hvm_jit_block *block = &bundle->blocks[i];
    if(block->ip == ip) {
      return block;
    }
  }
  assert(false);
  return NULL;
}

LLVMTypeRef hvm_jit_obj_ref_llvm_type() {
  static LLVMTypeRef strct;
  if(!strct) {
    LLVMContextRef context = hvm_get_llvm_context();
    strct = LLVMStructCreateNamed(context, "hvm_obj_ref");
    LLVMTypeRef type  = LLVMIntType(sizeof(hvm_obj_type) * 8);
    LLVMTypeRef data  = LLVMIntType(sizeof(union hvm_obj_ref_data) * 8);
    LLVMTypeRef flags = LLVMIntType(sizeof(byte) * 8);
    LLVMTypeRef entry = LLVMIntType(sizeof(void*) * 8);
    LLVMTypeRef body[4] = {type, data, flags, entry};
    // Last flag tells us it's not packed
    LLVMStructSetBody(strct, body, 4, false);
  }
  return strct;
}

LLVMTypeRef hvm_jit_exit_bailout_llvm_type() {
  static LLVMTypeRef strct;
  if(!strct) {
    LLVMContextRef context = hvm_get_llvm_context();
    strct = LLVMStructCreateNamed(context, "hvm_jit_exit_bailout");
    LLVMTypeRef status      = LLVMIntType(sizeof(hvm_jit_exit_status) * 8);
    LLVMTypeRef destination = hvm_jit_get_llvm_pointer_type();
    LLVMTypeRef body[2]     = {status, destination};
    LLVMStructSetBody(strct, body, 2, false);
  }
  return strct;
}

unsigned int hvm_jit_get_trace_index_for_ip(hvm_call_trace *trace, uint64_t ip, bool *found) {
  for(unsigned int index = 0; index < trace->sequence_length; index++) {
    hvm_trace_sequence_item *item = &trace->sequence[index];
    if(item->head.ip == ip) {
      *found = true;
      return index;
    }
  }
  *found = false;
  return 0;
}


LLVMValueRef hvm_jit_compile_value_is_falsey(LLVMBuilderRef builder, LLVMValueRef val_ref) {
  static LLVMTypeRef  obj_type_enum_size;
  static LLVMValueRef const_hvm_null;
  static LLVMValueRef const_hvm_integer;
  static LLVMTypeRef  int32_type;
  static LLVMTypeRef  int64_type;
  static LLVMValueRef i32_zero;
  static LLVMValueRef i32_one;
  static LLVMValueRef i64_zero;
  static bool defined;
  if(!defined) {
    obj_type_enum_size = LLVMIntType(sizeof(hvm_obj_type) * 8);
    const_hvm_null     = LLVMConstInt(obj_type_enum_size, HVM_NULL, false);
    const_hvm_integer  = LLVMConstInt(obj_type_enum_size, HVM_INTEGER, false);
    int32_type = LLVMInt32Type();
    int64_type = LLVMInt64Type();
    i32_zero   = LLVMConstInt(int32_type, 0, true);
    i32_one    = LLVMConstInt(int32_type, 1, true);
    i64_zero   = LLVMConstInt(int64_type, 0, true);
    defined = true;
  }

  // Get a pointer the the .type of the object ref struct (first 0 index
  // is to get the first value pointed at, the second 0 index is to get
  // the first item in the struct). Then load it into an integer value.
  LLVMValueRef val_type_ptr = LLVMBuildGEP(builder, val_ref, (LLVMValueRef[]){i32_zero, i32_zero}, 2, NULL);
  LLVMValueRef val_type     = LLVMBuildLoad(builder, val_type_ptr, NULL);
  // Get the .data of the object as an i64:
  LLVMValueRef val_data_ptr = LLVMBuildGEP(builder, val_ref, (LLVMValueRef[]){i32_zero, i32_one}, 2, NULL);
  LLVMValueRef val_data     = LLVMBuildLoad(builder, val_data_ptr, NULL);

  // Left side of the falsiness test
  LLVMValueRef val_is_null      = LLVMBuildICmp(builder, LLVMIntEQ, val_type, const_hvm_null, NULL);
  // Right side of the test (check if integer and i64-value is zero)
  LLVMValueRef val_is_int       = LLVMBuildICmp(builder, LLVMIntEQ, val_type, const_hvm_integer, NULL);
  LLVMValueRef val_data_is_zero = LLVMBuildICmp(builder, LLVMIntEQ, val_data, i64_zero, NULL);
  LLVMValueRef val_is_zero_int  = LLVMBuildAnd(builder, val_is_int, val_data_is_zero, NULL);

  // Final is-falsey computation
  LLVMValueRef falsey = LLVMBuildOr(builder, val_is_null, val_is_zero_int, NULL);
  return falsey;
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
  uint64_t ip;
  hvm_jit_block *jit_block;
  LLVMValueRef value, value_array, value_index, value_symbol, value_returned, value1, value2;

  hvm_compile_sequence_data *data = bundle->data;
  // Function-pointer-as-value
  LLVMValueRef func;

  LLVMBuilderRef builder = bundle->llvm_builder;
  // Get the top-level basic block and its parent function
  LLVMBasicBlockRef insert_block = LLVMGetInsertBlock(builder);
  LLVMValueRef      parent_func  = LLVMGetBasicBlockParent(insert_block);

  // Set up our generic pointer type (in the 0 address space)
  LLVMTypeRef pointer_type = hvm_jit_get_llvm_pointer_type();
  LLVMTypeRef int64_type   = LLVMInt64Type();

  for(i = 0; i < trace->sequence_length; i++) {
    hvm_compile_sequence_data *data_item  = &data[i];
    hvm_trace_sequence_item   *trace_item = &trace->sequence[i];

    // uint64_t current_ip = trace_item->head.ip;

    // TODO: Refactor this to be faster!
    hvm_jit_block    *current_block       = hvm_jit_get_current_block(bundle, i);
    LLVMBasicBlockRef current_basic_block = current_block->basic_block;
    // Make sure our builder is in the right place
    LLVMPositionBuilderAtEnd(builder, current_basic_block);

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
        value = LLVMConstInt(pointer_type, (unsigned long long)ref, false);
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
        value_array = general_reg_values[reg_array];
        // Getting the index value
        reg_index = trace_item->arrayget.register_index;
        assert(general_reg_values[reg_index] != NULL);
        value_index = general_reg_values[reg_index];
        // Get the function as a LLVM value we can work with
        func = hvm_jit_obj_array_get_llvm_value(bundle);
        LLVMValueRef arrayget_args[2] = {value_array, value_index};
        // Build the function call
        value_returned = LLVMBuildCall(builder, func, arrayget_args, 2, "arrayget");
        // Save the return value
        reg = trace_item->arrayget.register_value;
        JIT_SAVE_DATA_ITEM_AND_VALUE(reg, data_item, value_returned);
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
        LLVMValueRef arrayset_args[3] = {value_array, value_index, value};
        // Build the function call with the function value and arguments
        LLVMBuildCall(builder, func, arrayset_args, 3, "arrayset");
        break;

      case HVM_TRACE_SEQUENCE_ITEM_ARRAYLEN:
        data_item->head.type = HVM_COMPILE_DATA_ARRAYLEN;
        reg = trace_item->arraylen.register_value;
        // Get the source array to get the length of
        reg_array   = trace_item->arraylen.register_array;
        value_array = general_reg_values[reg_array];
        // Get the array-length function
        func = hvm_jit_obj_array_len_llvm_value(bundle);
        LLVMValueRef arraylen_args[1] = {value_array};
        // Then build the function call
        value_returned = LLVMBuildCall(builder, func, arraylen_args, 1, "arraylen");
        JIT_SAVE_DATA_ITEM_AND_VALUE(reg, data_item, value_returned);
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

      case HVM_TRACE_SEQUENCE_ITEM_GOTO:
        data_item->head.type = HVM_COMPILE_DATA_GOTO;
        // Look up the block we need to go to.
        jit_block = data_item->item_goto.destination_block;
        // Build the branch instruction to this block
        LLVMBuildBr(builder, jit_block->basic_block);
        break;

      case HVM_TRACE_SEQUENCE_ITEM_IF:
        data_item->item_if.type = HVM_COMPILE_DATA_IF;
        // Building our comparison:
        //   falsey = (val->type == HVM_NULL || (val->type == HVM_INTEGER && val->data.i64 == 0))
        // We only branch to the destination if it is not falsey (ie. truthy)

        // Extract the value we'll be testing and cast it to an hvm_obj_ref
        // type in the LLVM IR.
        value1               = general_reg_values[trace_item->item_if.register_value];
        LLVMValueRef val_ref = LLVMBuildPointerCast(builder, value1, hvm_jit_obj_ref_llvm_type(), NULL);
        // Expects `hvm_obj_ref` pointer and should return a bool LLVM value ref
        LLVMValueRef falsey = hvm_jit_compile_value_is_falsey(builder, val_ref);
        // Invert for our truthy test
        LLVMValueRef truthy = LLVMBuildNot(builder, falsey, NULL);

        // Get the TRUTHY block to branch to or set up a bailout
        LLVMBasicBlockRef truthy_block;
        if(data_item->item_if.truthy_block != NULL) {
          truthy_block = data_item->item_if.truthy_block->basic_block;
        } else {
          ip = trace_item->item_if.destination;
          truthy_block = hvm_jit_build_bailout_block(vm, builder, parent_func, general_reg_values, ip);
        }
        // Same for the FALSEY block
        LLVMBasicBlockRef falsey_block;
        if(data_item->item_if.falsey_block != NULL) {
          falsey_block = data_item->item_if.falsey_block->basic_block;
        } else {
          // Falsey just continues past the instruction
          ip = trace_item->head.ip + 10;
          falsey_block = hvm_jit_build_bailout_block(vm, builder, parent_func, general_reg_values, ip);
        }
        // And finally actually do the branch with those blocks
        LLVMBuildCondBr(builder, truthy, truthy_block, falsey_block);
        break;

      case HVM_TRACE_SEQUENCE_ITEM_LITINTEGER:
        data_item->head.type = HVM_COMPILE_DATA_LITINTEGER;
        reg = trace_item->litinteger.register_value;
        // Convert the literal integer value from the trace into an LLVM value
        // NOTE: Even though we're sending it as unsigned, the final `true`
        //       argument to LLVMConstInt should make sure LLVM knows it's
        //       actually signed.
        value = LLVMConstInt(int64_type, (unsigned long long)trace_item->litinteger.literal_value, true);
        // Save that value into the data and the "register"
        data_item->litinteger.value = value;
        data_item->litinteger.reg   = reg;
        // Then save the value into the "registers"
        JIT_SAVE_DATA_ITEM_AND_VALUE(reg, data_item, value);
        break;

      default:
        type = trace_item->head.type;
        fprintf(stderr, "jit-compiler: Don't know what to do with item type %d\n", type);
        break;
    }
  }
}

void hvm_jit_build_bailout_return_to_ip(LLVMBuilderRef builder, uint64_t ip) {
  static LLVMTypeRef int32_type;
  static LLVMTypeRef int64_type;
  static LLVMValueRef i32_zero;
  static bool types_defined;
  if(!types_defined) {
    int32_type = LLVMInt32Type();
    int64_type = LLVMInt64Type();
    i32_zero   = LLVMConstInt(int32_type, 0, true);
    types_defined = true;
  }

  // Get the bailout type and allocate the structure in the stack frame
  LLVMTypeRef bailout_type   = hvm_jit_exit_bailout_llvm_type();
  LLVMValueRef bailout_value = LLVMBuildAlloca(builder, bailout_type, NULL);
  // Initialize the values for the bailout struct (both unsigned)
  LLVMTypeRef  status_type  = LLVMIntType(sizeof(hvm_jit_exit_status) * 8);
  LLVMValueRef status_value = LLVMConstInt(status_type, HVM_JIT_EXIT_BAILOUT, false);
  LLVMValueRef dest_value   = LLVMConstInt(int64_type, ip, false);
  // Get the pointers to the struct elements
  LLVMValueRef status_ptr   = LLVMBuildGEP(builder, bailout_value, (LLVMValueRef[]){i32_zero, LLVMConstInt(int32_type, 0, true)}, 2, NULL);
  LLVMValueRef dest_ptr     = LLVMBuildGEP(builder, bailout_value, (LLVMValueRef[]){i32_zero, LLVMConstInt(int32_type, 1, true)}, 2, NULL);
  // And store the actual values in them
  LLVMBuildStore(builder, status_value, status_ptr);
  LLVMBuildStore(builder, dest_value,   dest_ptr);
  // Then dereference the whole struct so we can return it
  LLVMValueRef bailout_ptr   = LLVMBuildGEP(builder, bailout_value, (LLVMValueRef[]){i32_zero}, 1, NULL);
  LLVMValueRef bailout_strct = LLVMBuildLoad(builder, bailout_ptr, NULL);
  LLVMBuildRet(builder, bailout_strct);
}


LLVMBasicBlockRef hvm_jit_build_bailout_block(hvm_vm *vm, LLVMBuilderRef builder, LLVMValueRef parent_func, LLVMValueRef *general_reg_values, uint64_t ip) {
  static LLVMTypeRef pointer_type;
  static LLVMTypeRef int32_type;
  static bool types_defined;
  if(!types_defined) {
    pointer_type = hvm_jit_get_llvm_pointer_type();
    int32_type   = LLVMInt32Type();
    types_defined = true;
  }
  
  LLVMContextRef context = hvm_get_llvm_context();
  // Create the basic block for our bailout code
  LLVMBasicBlockRef basic_block = LLVMAppendBasicBlockInContext(context, parent_func, NULL);
  LLVMPositionBuilderAtEnd(builder, basic_block);

  // Get the pointer to the VM registers
  hvm_obj_ref **general_regs = vm->general_regs;
  // Then convert that to an LLVM pointer
  LLVMValueRef general_regs_ptr = LLVMConstInt(pointer_type, (unsigned long long)general_regs, false);

  // Loop over each computed general reg value and copy that into the VM's
  // general regs.
  for(unsigned int i = 0; i < HVM_GENERAL_REGISTERS; i++) {
    LLVMValueRef value = general_reg_values[i];
    if(value == NULL) {
      // Don't need to worry about copying NULLs
      continue;
    }
    LLVMValueRef idx_val = LLVMConstInt(int32_type, i, true);
    // Get the pointer to the item in the pointer array
    LLVMValueRef reg_ptr = LLVMBuildGEP(builder, general_regs_ptr, (LLVMValueRef[]){idx_val}, 1, NULL);
    // Now actually copy the value into the register
    LLVMBuildStore(builder, value, reg_ptr);
  }
  // TODO: Also copy argument registers!

  // Build the return of the `hvm_jit_exit` structure-union from the JIT code
  // segment/function.
  hvm_jit_build_bailout_return_to_ip(builder, ip);

  return basic_block;
}


hvm_jit_block *hvm_jit_compile_find_or_insert_block(LLVMValueRef parent_func, hvm_compile_bundle *bundle, unsigned int index, uint64_t ip) {
  hvm_jit_block *blocks   = bundle->blocks;
  unsigned int num_blocks = bundle->blocks_length;
  LLVMContextRef context  = hvm_get_llvm_context();
  // Block to be operated upon
  hvm_jit_block *block;

  // TODO: Optimize this to look at the end of the blocks array to see if the
  //       IP of the block to be inserted is greater than the last block's IP.
  //       If this is true then we can probably skip the search.

  for(unsigned int i = 0; i < num_blocks; i++) {
    block = &blocks[i];
    // Don't duplicate blocks
    if(ip == block->ip) {
      return block;
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
  // Update the count
  bundle->blocks_length = num_blocks + 1;
  // Return the new block
  return block;
}

bool hvm_jit_trace_contains_ip(hvm_call_trace *trace, uint64_t ip) {
  for(unsigned int i = 0; i < trace->sequence_length; i++) {
    hvm_trace_sequence_item *item = &trace->sequence[i];
    if(item->head.ip == ip) {
      return true;
    }
  }
  return false;
}

void hvm_jit_compile_identify_blocks(hvm_call_trace *trace, hvm_compile_bundle *bundle) {
  // Sanity guards
  assert(trace->sequence_length > 0);

  hvm_jit_block *blocks = malloc(sizeof(hvm_jit_block) * trace->sequence_length);
  // Set up the blocks and length in the bundle
  bundle->blocks        = blocks;
  bundle->blocks_length = 1;// See entry point below

  bool found;
  unsigned int index;
  uint64_t ip;
  hvm_trace_sequence_item *item;
  hvm_compile_sequence_data *data_item;
  hvm_jit_block *block;

  // Unpack the bundle and get the context
  LLVMBuilderRef builder = bundle->llvm_builder;
  LLVMContextRef context = hvm_get_llvm_context();
  // Get the top-level basic block and its parent function
  LLVMBasicBlockRef insert_block = LLVMGetInsertBlock(builder);
  LLVMValueRef      parent_func  = LLVMGetBasicBlockParent(insert_block);

  // Get the first item and set up a block based off of it for entry
  item = &trace->sequence[0];
  // Set up a block for our entry point
  hvm_jit_block *entry = &blocks[0];
  entry->ip            = item->head.ip;
  entry->index         = 0;
  entry->basic_block   = LLVMAppendBasicBlockInContext(context, parent_func, NULL);

  for(unsigned int i = 0; i < trace->sequence_length; i++) {
    item = &trace->sequence[i];
    // Also need to be able to set some stuff on the data item in this pass
    data_item = &bundle->data[i];

    switch(item->head.type) {
      case HVM_OP_IF:
        // Handle the truthy destination
        ip = item->item_if.destination;
        if(hvm_jit_trace_contains_ip(trace, ip)) {
          // Set up a TRUTHINESS block for the IP if it was found
          block = hvm_jit_compile_find_or_insert_block(parent_func, bundle, i, ip);
          data_item->item_if.truthy_block = block;
        } else {
          // Otherwise set the truthy block to NULL so that we know to insert
          // a bailout
          data_item->item_if.truthy_block = NULL;
        }

        found = false;
        ip    = item->head.ip + 10;// 10 bytes for op, register, and destination
        index = hvm_jit_get_trace_index_for_ip(trace, ip, &found);
        // If we found a block:
        if(found) {
          // Setting up a FALSINESS block
          block = hvm_jit_compile_find_or_insert_block(parent_func, bundle, index, ip);
          data_item->item_if.falsey_block = block;  
        } else {
          // Otherwise set to NULL to indicate we need a bailout
          data_item->item_if.falsey_block = NULL;
        }
        break;

      case HVM_OP_GOTO:
        ip = item->item_goto.destination;
        assert(hvm_jit_trace_contains_ip(trace, ip));// TODO: Generate bailout
        block = hvm_jit_compile_find_or_insert_block(parent_func, bundle, i, ip);
        data_item->item_goto.destination_block = block;
        break;

      default:
        // Don't do anything for other items
        continue;
    }
  }
}

int hvm_trace_item_comparator(const void *va, const void *vb) {
  hvm_trace_sequence_item *a = (hvm_trace_sequence_item*)va;
  hvm_trace_sequence_item *b = (hvm_trace_sequence_item*)vb;
  int aip = (int)a->head.ip;
  int bip = (int)b->head.ip;
  return aip - bip;
}

void hvm_jit_sort_trace(hvm_call_trace *trace) {
  qsort(trace->sequence, trace->sequence_length, sizeof(hvm_trace_sequence_item), &hvm_trace_item_comparator);
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

  // Rearrange the trace to be a linear sequence of ordered IPs
  hvm_jit_sort_trace(trace);

  // Break our sequence into blocks based upon possible destinations of
  // jumps/ifs/gotos/etc.
  hvm_jit_compile_identify_blocks(trace, &bundle);

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
