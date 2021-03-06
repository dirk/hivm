
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/Transforms/Scalar.h>

#include <jemalloc/jemalloc.h>

#include "vm.h"
#include "symbol.h"
#include "object.h"
#include "frame.h"
#include "bootstrap.h"
#include "jit-tracer.h"
#include "jit-compiler.h"

// Forward declarations for a few things
LLVMTypeRef hvm_jit_obj_ref_llvm_type();

static bool constants_defined;
// Static constants we'll be using
static LLVMTypeRef  obj_type_enum_type;
// Types
static LLVMTypeRef  pointer_type;
static LLVMTypeRef  void_type;
static LLVMTypeRef  byte_type;
static LLVMTypeRef  bool_type;
static LLVMTypeRef  int1_type;
static LLVMTypeRef  int32_type;
static LLVMTypeRef  int64_type;
static LLVMTypeRef  int64_pointer_type;
// Complex types
static LLVMTypeRef obj_ref_ptr_type;
// Some values we'll be reusing a lot
static LLVMValueRef i32_zero;
static LLVMValueRef i32_one;
static LLVMValueRef i64_zero;
// Integer values of the HVM_NULL and HVM_INTEGER enum items
static LLVMValueRef const_hvm_null;
static LLVMValueRef const_hvm_integer;

// Setting up the internals
static bool llvm_setup;
// Reuse our compilation context and module through program lifetime
static LLVMContextRef         hvm_shared_llvm_context;
static LLVMModuleRef          hvm_shared_llvm_module;
static LLVMExecutionEngineRef hvm_shared_llvm_engine;
static LLVMPassManagerRef     hvm_shared_llvm_pass_manager;

void hvm_jit_define_constants() {
  if(constants_defined) {
    return;// Don't redefine!
  }
  obj_type_enum_type = LLVMIntTypeInContext(hvm_shared_llvm_context, sizeof(hvm_obj_type) * 8);
  const_hvm_null     = LLVMConstInt(obj_type_enum_type, HVM_NULL, false);
  const_hvm_integer  = LLVMConstInt(obj_type_enum_type, HVM_INTEGER, false);
  void_type          = LLVMVoidTypeInContext(hvm_shared_llvm_context);
  byte_type          = LLVMInt8TypeInContext(hvm_shared_llvm_context);
  bool_type          = LLVMIntTypeInContext(hvm_shared_llvm_context, sizeof(bool) * 8);
  int1_type          = LLVMInt1TypeInContext(hvm_shared_llvm_context);
  int32_type         = LLVMInt32TypeInContext(hvm_shared_llvm_context);
  int64_type         = LLVMInt64TypeInContext(hvm_shared_llvm_context);
  int64_pointer_type = LLVMPointerType(int64_type, 0);
  pointer_type       = LLVMPointerType(LLVMInt8TypeInContext(hvm_shared_llvm_context), 0);
  i32_zero           = LLVMConstInt(int32_type, 0, true);
  i32_one            = LLVMConstInt(int32_type, 1, true);
  i64_zero           = LLVMConstInt(int64_type, 0, true);
  // More complex types
  obj_ref_ptr_type   = LLVMPointerType(hvm_jit_obj_ref_llvm_type(), 0);
  // Mark that we've defined them so we don't redefine
  constants_defined = true;
}

void hvm_jit_setup_llvm() {
  if(llvm_setup) {
    return;// Don't re-setup our LLVM stuff
  }
  char *error;
  LLVMBool status;
  LLVMPassManagerRef pass;
  // Set up our context and module
  hvm_shared_llvm_context = LLVMContextCreate();
  hvm_shared_llvm_module  = LLVMModuleCreateWithNameInContext("hvm", hvm_shared_llvm_context);
  // Set up compilation to our current native target
  assert(LLVMInitializeNativeTarget() == 0);
  assert(LLVMInitializeNativeAsmPrinter() == 0);
  LLVMLinkInMCJIT();
  // Set up the options for the JIT compiler
  struct LLVMMCJITCompilerOptions opts;
  LLVMInitializeMCJITCompilerOptions(&opts, sizeof(opts));
  // Set up an engine
  status = LLVMCreateMCJITCompilerForModule(&hvm_shared_llvm_engine, hvm_shared_llvm_module, &opts, sizeof(opts), &error);
  if(status != 0) {
    fprintf(stderr, "Error instantiating execution engine: %s\n", error);
    assert(false);
  }
  // Set up our pass manager
  hvm_shared_llvm_pass_manager = LLVMCreateFunctionPassManagerForModule(hvm_shared_llvm_module);
  pass = hvm_shared_llvm_pass_manager;// Save our fingers!
  LLVMAddTargetData(LLVMGetExecutionEngineTargetData(hvm_shared_llvm_engine), pass);
  // Constant propagation simplifies/removes-unnecessary computations of
  // constant values.
  LLVMAddConstantPropagationPass(pass);
  // Does simple "peephole" algebraic simplifications.
  LLVMAddInstructionCombiningPass(pass);
  // This pass reassociates commutative expressions to make life easier for
  // the next passes. Example from LLVM docs:
  //   4 + (x + 5) -> x + (4 + 5)
  LLVMAddReassociatePass(pass);
  // Promotes memory references to register references where possible to
  // avoid unnecessary load/stores.
  LLVMAddPromoteMemoryToRegisterPass(pass);
  // Provide basic AliasAnalysis support for the GVN pass.
  LLVMAddBasicAliasAnalysisPass(pass);
  // Do global value numbering to eliminate fully redundant instructions and
  // dead lods.
  LLVMAddGVNPass(pass);
  // Simplify control flow graph to remove dead code, merge basic blocks, etc.
  LLVMAddCFGSimplificationPass(pass);

  // Now that we've added all our passes we can initialize the FPM so that it
  // will be ready to run on values in our module.
  LLVMInitializeFunctionPassManager(pass);
}


#define UNPACK_BUNDLE(BUNDLE) \
  LLVMModuleRef module          = BUNDLE->llvm_module; \
  LLVMExecutionEngineRef engine = BUNDLE->llvm_engine;

#define STATIC_VALUE(TYPE, NAME) \
  static TYPE NAME; \
  if(NAME) { return NAME; }

// NOTE: Last argument to LLVMFunctionType tells LLVM it's non-variadic.
#define ADD_FUNCTION(FUNC, EXT_FUNCTION, RETURN_TYPE, NUM_PARAMS, PARAM_TYPES...) \
  { \
    LLVMTypeRef param_types[] = {PARAM_TYPES}; \
    LLVMTypeRef func_type = LLVMFunctionType(RETURN_TYPE, param_types, NUM_PARAMS, false); \
    FUNC = LLVMAddFunction(module, #EXT_FUNCTION, func_type); \
    LLVMAddGlobalMapping(engine, func, &EXT_FUNCTION); \
  }

LLVMValueRef hvm_jit_obj_array_get_llvm_value(hvm_compile_bundle *bundle) {
  STATIC_VALUE(LLVMValueRef, func);
  UNPACK_BUNDLE(bundle);
  ADD_FUNCTION(func, hvm_obj_array_get, obj_ref_ptr_type, 2, obj_ref_ptr_type, obj_ref_ptr_type);
  return func;
}

LLVMValueRef hvm_jit_obj_array_set_llvm_value(hvm_compile_bundle *bundle) {
  STATIC_VALUE(LLVMValueRef, func);
  UNPACK_BUNDLE(bundle);
  ADD_FUNCTION(func, hvm_obj_array_set, void_type, 3, obj_ref_ptr_type, obj_ref_ptr_type, obj_ref_ptr_type);
  return func;
}

LLVMValueRef hvm_jit_obj_array_len_llvm_value(hvm_compile_bundle *bundle) {
  STATIC_VALUE(LLVMValueRef, func);
  UNPACK_BUNDLE(bundle);
  // (hvm_vm*, hvm_obj_ref*) -> hvm_obj_ref*
  ADD_FUNCTION(func, hvm_obj_array_len, obj_ref_ptr_type, 2, pointer_type, obj_ref_ptr_type);
  return func;
}

LLVMValueRef hvm_jit_vm_call_primitive_llvm_value(hvm_compile_bundle *bundle) {
  STATIC_VALUE(LLVMValueRef, func);
  UNPACK_BUNDLE(bundle);
  // (hvm_vm*, hvm_obj_ref*) -> hvm_obj_ref*
  ADD_FUNCTION(func, hvm_vm_call_primitive, obj_ref_ptr_type, 2, pointer_type, obj_ref_ptr_type);
  return func;
}

LLVMValueRef hvm_jit_obj_cmp_and_llvm_value(hvm_compile_bundle *bundle) {
  STATIC_VALUE(LLVMValueRef, func);
  UNPACK_BUNDLE(bundle);
  // (hvm_vm*, hvm_obj_ref*, hvm_obj_ref*) -> hvm_obj_ref*
  ADD_FUNCTION(func, hvm_obj_cmp_and, obj_ref_ptr_type, 3, pointer_type, obj_ref_ptr_type, obj_ref_ptr_type);
  return func;
}

LLVMValueRef hvm_jit_vm_copy_regs_llvm_value(hvm_compile_bundle *bundle) {
  STATIC_VALUE(LLVMValueRef, func);
  UNPACK_BUNDLE(bundle);
  // (hvm_vm*) -> void
  ADD_FUNCTION(func, hvm_vm_copy_regs, void_type, 1, pointer_type);
  return func;
}

LLVMValueRef hvm_jit_obj_int_add_llvm_value(hvm_compile_bundle *bundle) {
  STATIC_VALUE(LLVMValueRef, func);
  UNPACK_BUNDLE(bundle);
  ADD_FUNCTION(func, hvm_obj_int_add, obj_ref_ptr_type, 3, pointer_type, obj_ref_ptr_type, obj_ref_ptr_type);
  return func;
}

LLVMValueRef hvm_jit_obj_int_eq_llvm_value(hvm_compile_bundle *bundle) {
  STATIC_VALUE(LLVMValueRef, func);
  UNPACK_BUNDLE(bundle);
  ADD_FUNCTION(func, hvm_obj_int_eq, obj_ref_ptr_type, 3, pointer_type, obj_ref_ptr_type, obj_ref_ptr_type);
  return func;
}

LLVMValueRef hvm_jit_obj_int_gt_llvm_value(hvm_compile_bundle *bundle) {
  STATIC_VALUE(LLVMValueRef, func);
  UNPACK_BUNDLE(bundle);
  ADD_FUNCTION(func, hvm_obj_int_gt, obj_ref_ptr_type, 3, pointer_type, obj_ref_ptr_type, obj_ref_ptr_type);
  return func;
}

LLVMValueRef hvm_jit_obj_is_truthy_llvm_value(hvm_compile_bundle *bundle) {
  STATIC_VALUE(LLVMValueRef, func);
  UNPACK_BUNDLE(bundle);
  // (hvm_obj_ref*) -> bool
  ADD_FUNCTION(func, hvm_obj_is_truthy, bool_type, 1, obj_ref_ptr_type);
  return func;
}

LLVMValueRef hvm_jit_set_local_llvm_value(hvm_compile_bundle *bundle) {
  STATIC_VALUE(LLVMValueRef, func);
  UNPACK_BUNDLE(bundle);
  // (hvm_frame*, hvm_symbol_id, hvm_obj_ref*) -> void
  ADD_FUNCTION(func, hvm_set_local, void_type, 3, pointer_type, int64_type, obj_ref_ptr_type);
  return func;
}

LLVMValueRef hvm_jit_get_local_llvm_value(hvm_compile_bundle *bundle) {
  STATIC_VALUE(LLVMValueRef, func);
  UNPACK_BUNDLE(bundle);
  // (hvm_frame*, hvm_symbol_id) -> hvm_obj_ref*
  ADD_FUNCTION(func, hvm_get_local, obj_ref_ptr_type, 2, pointer_type, int64_type);
  return func;
}

LLVMValueRef hvm_jit_new_obj_int_llvm_value(hvm_compile_bundle *bundle) {
  STATIC_VALUE(LLVMValueRef, func);
  UNPACK_BUNDLE(bundle);
  // (hvm_vm*) -> hvm_obj_ref*
  LLVMTypeRef param_types[] = {pointer_type};
  LLVMTypeRef func_type = LLVMFunctionType(pointer_type, param_types, 1, false);
  func = LLVMAddFunction(module, "hvm_new_obj_int", func_type);
  LLVMAddGlobalMapping(engine, func, &hvm_new_obj_int);
  return func;
}

LLVMValueRef hvm_jit_puts_llvm_value(hvm_compile_bundle *bundle) {
  STATIC_VALUE(LLVMValueRef, func);
  UNPACK_BUNDLE(bundle);
  ADD_FUNCTION(func, puts, void_type, 1, pointer_type);
  return func;
}

void hvm_jit_llvm_print_string(hvm_compile_bundle *bundle, LLVMBuilderRef builder, char *string) {
  static LLVMBool dont_null_terminate = false;
  unsigned long  length   = strlen(string);
  LLVMContextRef context  = hvm_shared_llvm_context;
  LLVMValueRef   string_const = LLVMConstStringInContext(context, string, (unsigned int)length, dont_null_terminate);
  // Store the string constant we just made in the module globals.
  char scratch[80];
  sprintf(scratch, "print_string_%p", string);
  LLVMValueRef string_global = LLVMAddGlobal(bundle->llvm_module, LLVMTypeOf(string_const), scratch);
  LLVMSetLinkage(string_global, LLVMPrivateLinkage);
  LLVMSetGlobalConstant(string_global, true);
  LLVMSetInitializer(string_global, string_const);
  // Now get a pointer to that string global and pass it to `puts`.
  LLVMValueRef string_ptr = LLVMBuildInBoundsGEP(builder, string_global, (LLVMValueRef[]){i32_zero, i32_zero}, 2, "");
  LLVMValueRef func = hvm_jit_puts_llvm_value(bundle);
  LLVMValueRef puts_args[1] = {string_ptr};
  // Finally call `puts`.
  LLVMBuildCall(builder, func, puts_args, 1, "");
}

hvm_jit_block *hvm_jit_get_current_block(hvm_compile_bundle *bundle, uint64_t ip) {
  hvm_jit_block *block = bundle->blocks_head;

  while(true) {
    hvm_jit_block *next = block->next;
    // Return this block if we've run out of blocks
    if(next == NULL) {
      return block;
    }
    // Return this block if the next block's IP is greater than what
    // we're looking for.
    if(next->ip > ip) {
      return block;
    }
    // Otherwise advance
    block = next;
  }
  // Unreachable
  assert(false);
  return NULL;
}

hvm_jit_block *hvm_jit_get_block_by_ip(hvm_compile_bundle *bundle, uint64_t ip) {
  hvm_jit_block *block = bundle->blocks_head;
  while(block != NULL) {
    if(block->ip == ip) { return block; }
    block = block->next;
  }
  fprintf(stderr, "Failed to get block with IP %08llX\n", ip);
  assert(false);
  return NULL;
}

LLVMTypeRef hvm_jit_obj_ref_llvm_type() {
  STATIC_VALUE(LLVMTypeRef, strct);
  strct = LLVMStructCreateNamed(hvm_shared_llvm_context, "hvm_obj_ref");
  LLVMTypeRef type  = LLVMIntType(sizeof(hvm_obj_type) * 8);
  LLVMTypeRef data  = LLVMIntType(sizeof(union hvm_obj_ref_data) * 8);
  LLVMTypeRef flags = LLVMIntType(sizeof(byte) * 8);
  LLVMTypeRef entry = LLVMIntType(sizeof(void*) * 8);
  LLVMTypeRef body[4] = {type, data, flags, entry};
  // Last flag tells LLVM it's not packed
  LLVMStructSetBody(strct, body, 4, false);
  return strct;
}

LLVMTypeRef hvm_jit_exit_bailout_llvm_type() {
  STATIC_VALUE(LLVMTypeRef, strct);
  strct = LLVMStructCreateNamed(hvm_shared_llvm_context, "hvm_jit_exit_bailout");
  LLVMTypeRef status      = LLVMIntType(sizeof(hvm_jit_exit_status) * 8);
  LLVMTypeRef destination = int64_type;
  LLVMTypeRef body[2]     = {status, destination};
  LLVMStructSetBody(strct, body, 2, false);
  return strct;
}

LLVMTypeRef hvm_jit_exit_return_llvm_type() {
  STATIC_VALUE(LLVMTypeRef, strct);
  strct = LLVMStructCreateNamed(hvm_shared_llvm_context, "hvm_jit_exit_return");
  LLVMTypeRef status  = LLVMIntType(sizeof(hvm_jit_exit_status) * 8);
  LLVMTypeRef value   = obj_ref_ptr_type;
  LLVMTypeRef body[2] = {status, value};
  LLVMStructSetBody(strct, body, 2, false);
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
  // Get a pointer the the .type of the object ref struct (first 0 index
  // is to get the first value pointed at, the second 0 index is to get
  // the first item in the struct). Then load it into an integer value.
  LLVMValueRef val_type_ptr = LLVMBuildGEP(builder, val_ref, (LLVMValueRef[]){i32_zero, i32_zero}, 2, "val_type_ptr");
  LLVMValueRef val_type     = LLVMBuildLoad(builder, val_type_ptr, "");
  val_type                  = LLVMBuildIntCast(builder, val_type, obj_type_enum_type, "val_type");
  // Get the .data of the object as an i64:
  LLVMValueRef val_data_ptr = LLVMBuildGEP(builder, val_ref, (LLVMValueRef[]){i32_zero, i32_one}, 2, "val_data_ptr");
  LLVMValueRef val_data     = LLVMBuildLoad(builder, val_data_ptr, "");
  val_data                  = LLVMBuildIntCast(builder, val_data, int64_type, "val_data");

  // fprintf(stderr, "val_ref: %p %s\n", LLVMTypeOf(val_type), LLVMPrintTypeToString(LLVMTypeOf(val_type)));
  // fprintf(stderr, "const_hvm_null: %p %s\n", LLVMTypeOf(const_hvm_null), LLVMPrintTypeToString(LLVMTypeOf(const_hvm_null)));
  // Left side of the falsiness test
  LLVMValueRef val_is_null      = LLVMBuildICmp(builder, LLVMIntEQ, val_type, const_hvm_null, "");
  // Right side of the test (check if integer and i64-value is zero)
  LLVMValueRef val_is_int       = LLVMBuildICmp(builder, LLVMIntEQ, val_type, const_hvm_integer, "");
  LLVMValueRef val_data_is_zero = LLVMBuildICmp(builder, LLVMIntEQ, val_data, i64_zero, "");
  // Cast test results to make sure they're bools
  val_is_int       = LLVMBuildIntCast(builder, val_is_int,       int1_type, "val_is_int");
  val_data_is_zero = LLVMBuildIntCast(builder, val_data_is_zero, int1_type, "val_data_is_zero");
  // Then compare those bools
  LLVMValueRef val_is_zero_int  = LLVMBuildAnd(builder, val_is_int, val_data_is_zero, "");

  // Final is-falsey computation
  val_is_null         = LLVMBuildIntCast(builder, val_is_null,     int1_type, "val_is_null");
  val_is_zero_int     = LLVMBuildIntCast(builder, val_is_zero_int, int1_type, "val_is_zero_int");
  LLVMValueRef falsey = LLVMBuildOr(builder, val_is_null, val_is_zero_int, "falsey");
  return falsey;
}

LLVMValueRef hvm_jit_load_symbol_id_from_obj_ref_value(LLVMBuilderRef builder, LLVMValueRef value) {
  LLVMValueRef data_ptr = LLVMBuildGEP(builder, value, (LLVMValueRef[]){i32_zero, i32_one}, 2, "data_ptr");
  LLVMValueRef data     = LLVMBuildLoad(builder, data_ptr, "");
  // Fetch out the .data as an int64 (same as hvm_symbol_id)
  return LLVMBuildIntCast(builder, data, int64_type, "data");
}

// #define JIT_SAVE_DATA_ITEM_AND_VALUE(REG, DATA_ITEM, VALUE) \
//   if(REG <= 127) { \
//     general_reg_data_sources[REG] = DATA_ITEM; \
//     general_reg_values[REG]       = VALUE; \
//   } else { \
//     fprintf(stderr, "jit-compiler: Cannot handle write to register type %d\n", REG); \
//     assert(false); \
//   }

void hvm_jit_store_value(struct hvm_jit_compile_context *context, hvm_compile_value *value) {
  // Store in the values
  hvm_compile_value **values = context->values;
  byte reg = value->reg;
  values[reg] = value;
}
hvm_compile_value *hvm_jit_get_value(struct hvm_jit_compile_context *context, byte reg) {
  return context->values[reg];
}

hvm_compile_value *hvm_compile_value_new(char type, byte reg) {
  hvm_compile_value *cv = je_malloc(sizeof(hvm_compile_value));
  cv->reg  = reg;
  cv->type = type;
  cv->constant = false;
  cv->constant_object = NULL;
  return cv;
}


// Slots are pointers to values on the stack; those values on the stack then
// contain a pointer of type hvm_obj_ref*
LLVMValueRef hvm_jit_load_slot(LLVMBuilderRef builder, LLVMValueRef slot, char *name) {
  // Get a pointer to the location on the stack
  LLVMValueRef ptr = LLVMBuildGEP(builder, slot, (LLVMValueRef[]){i32_zero}, 1, "");
  // Then load the hvm_obj_ref* pointer from that location
  return LLVMBuildLoad(builder, ptr, name);
}

// Put a given hvm_obj_ref* (wrapped in a LLVMValueRef) in a slot on the stack
void hvm_jit_store_slot(LLVMBuilderRef builder, LLVMValueRef slot, LLVMValueRef value, char *name) {
  // Get the pointer to the slot
  LLVMValueRef ptr = LLVMBuildGEP(builder, slot, (LLVMValueRef[]){i32_zero}, 1, name);
  // Put the hvm_obj_ref* pointer in that slot
  LLVMBuildStore(builder, value, ptr);
}

LLVMValueRef hvm_jit_load_general_reg_value(struct hvm_jit_compile_context *context, LLVMBuilderRef builder, byte reg) {
  if(reg > 127) {
    fprintf(stderr, "jit-compiler: Cannot handle load from register %d\n", reg);
    assert(false);
  }
  LLVMValueRef gr = context->general_regs[reg];
  assert(gr != NULL);
  char scratch[40];
  sprintf(scratch, "general_reg[%d]", reg);
  return hvm_jit_load_slot(builder, gr, scratch);
}

void hvm_jit_store_general_reg_value(struct hvm_jit_compile_context*, LLVMBuilderRef, byte, LLVMValueRef);
void hvm_jit_store_arg_reg_value(struct hvm_jit_compile_context*, LLVMBuilderRef, byte, LLVMValueRef);

void hvm_jit_store_reg_value(struct hvm_jit_compile_context *context, LLVMBuilderRef builder, byte reg, LLVMValueRef value) {
  if(hvm_is_gen_reg(reg)) {
    hvm_jit_store_general_reg_value(context, builder, reg, value);
  } else if(hvm_is_arg_reg(reg)) {
    hvm_jit_store_arg_reg_value(context, builder, reg, value);
  } else if(reg == hvm_vm_reg_null()) {
    return;// Do nothing if writing to null register
  } else {
    fprintf(stderr, "jit-compiler: Cannot handle write to register %d\n", reg);
    assert(false);
  }
}

void hvm_jit_store_general_reg_value(struct hvm_jit_compile_context *context, LLVMBuilderRef builder, byte reg, LLVMValueRef value) {
  if(reg > 127) {
    fprintf(stderr, "jit-compiler: Bad general register write: reg = %d\n", reg);
    assert(false);
  }
  LLVMValueRef gr = context->general_regs[reg];
  char scratch[40];
  sprintf(scratch, "general_reg[%d]", reg);
  if(gr == NULL) {
    // Allocate the register on the stack if it's null
    gr = LLVMBuildAlloca(builder, obj_ref_ptr_type, scratch);
    // And also save it for future loads and stores
    context->general_regs[reg] = gr;
  }
  LLVMValueRef slot = gr;
  hvm_jit_store_slot(builder, slot, value, scratch);
}

void hvm_jit_store_arg_reg_value(struct hvm_jit_compile_context *context, LLVMBuilderRef builder, byte reg, LLVMValueRef value) {
  // `value` should be an object reference pointer
  LLVMValueRef args_ptr, arg_ptr;
  // Figure out our offset
  unsigned int offset = reg - HVM_REG_ARG_OFFSET;
  char scratch[40];
  sprintf(scratch, "arg_reg[%d]", offset);

  static LLVMTypeRef ptr_to_obj_ptr_type;
  if(!ptr_to_obj_ptr_type) {
    ptr_to_obj_ptr_type = LLVMPointerType(obj_ref_ptr_type, 0);
  }

  // Get the base of the argument registers array
  hvm_obj_ref **arg_regs = context->vm->arg_regs;
  // Make a constant pointer to that base of the array
  args_ptr = LLVMConstInt(int64_type, (unsigned long long)arg_regs, false);
  args_ptr = LLVMBuildIntToPtr(builder, args_ptr, ptr_to_obj_ptr_type, "args");
  // Offset into that array
  LLVMValueRef offset_value = LLVMConstInt(int32_type, offset, false);
  arg_ptr = LLVMBuildGEP(builder, args_ptr, (LLVMValueRef[]){offset_value}, 1, scratch);
  // Write the contents of the value pointer into the VM's argument register
  LLVMBuildStore(builder, value, arg_ptr);
}

LLVMValueRef hvm_llvm_value_for_obj_ref(LLVMBuilderRef builder, hvm_obj_ref *ref) {
  LLVMValueRef ptr = LLVMConstInt(int64_type, (unsigned long long)ref, false);
  return LLVMBuildIntToPtr(builder, ptr, obj_ref_ptr_type, "");
}

LLVMValueRef hvm_jit_obj_int_add_direct(struct hvm_jit_compile_context *context, LLVMBuilderRef builder, LLVMValueRef vm_ptr, hvm_compile_value *cv1, hvm_compile_value *cv2, byte reg1, byte reg2) {
  LLVMValueRef func, value, data_ptr, data_ptr1, data_ptr2, value1, value2;
  // Get the bundle from the context
  hvm_compile_bundle *bundle = context->bundle;
  // printf("$%d = $%d + $%d\n", register_result, reg1, reg2);
  // printf("cv1->constant = %d\n", cv1->constant);
  // printf("cv2->constant = %d\n", cv2->constant);
  // printf("cv1->constant_object = %p\n", cv1->constant_object);
  // printf("cv2->constant_object = %p\n", cv2->constant_object);

  // If the left side is constant in the scope and has an unchanging value
  if(context->constant_regs[reg1] && cv1->constant) {
    // printf("using constant object for LHS\n");
    value1 = hvm_llvm_value_for_obj_ref(builder, cv1->constant_object);
  } else {
    // Insert code to extract the operand object refs from the stack slots
    value1 = hvm_jit_load_general_reg_value(context, builder, reg1);
  }
  // Same for right side
  if(context->constant_regs[reg2] && cv2->constant) {
    // printf("using constant object for RHS\n");
    value2 = hvm_llvm_value_for_obj_ref(builder, cv2->constant_object);
  } else {
    value2 = hvm_jit_load_general_reg_value(context, builder, reg2);
  }
  // Get the data pointer and fetch it
  data_ptr1 = LLVMBuildGEP(builder, value1, (LLVMValueRef[]){i32_zero, i32_one}, 2, "");
  data_ptr2 = LLVMBuildGEP(builder, value2, (LLVMValueRef[]){i32_zero, i32_one}, 2, "");
  LLVMValueRef operand1, operand2;
  operand1 = LLVMBuildLoad(builder, data_ptr1, "");
  operand2 = LLVMBuildLoad(builder, data_ptr2, "");
  // Then cast them to i64
  operand1 = LLVMBuildIntCast(builder, operand1, int64_type, "operand1");
  operand2 = LLVMBuildIntCast(builder, operand2, int64_type, "operand2");
  // Add the values
  LLVMValueRef value_i64 = LLVMBuildAdd(builder, operand1, operand2, "value");
  // Create the integer to return
  func = hvm_jit_new_obj_int_llvm_value(bundle);
  LLVMValueRef new_obj_int_args[1] = {vm_ptr};
  value = LLVMBuildCall(builder, func, new_obj_int_args, 1, "obj_ref_int");
  value = LLVMBuildPointerCast(builder, value, obj_ref_ptr_type, "value");
  // Get the pointer into its data and set the new value
  data_ptr = LLVMBuildGEP(builder, value, (LLVMValueRef[]){i32_zero, i32_one}, 2, "");
  data_ptr = LLVMBuildPointerCast(builder, data_ptr, int64_pointer_type, "value->data.i64");
  LLVMBuildStore(builder, value_i64, data_ptr);
  return value;
}

void hvm_jit_build_bailout_return_to_ip(LLVMBuilderRef builder, LLVMValueRef exit_value, uint64_t ip) {
  // Initialize the values for the bailout struct (both unsigned)
  LLVMTypeRef  status_type  = LLVMIntType(sizeof(hvm_jit_exit_status) * 8);
  LLVMValueRef status_value = LLVMConstInt(status_type, HVM_JIT_EXIT_BAILOUT, false);
  LLVMValueRef dest_value   = LLVMConstInt(int64_type, ip, false);
  // Get the pointers to the struct elements
  LLVMValueRef status_ptr   = LLVMBuildGEP(builder, exit_value, (LLVMValueRef[]){i32_zero, i32_zero}, 2, NULL);
  LLVMValueRef dest_ptr     = LLVMBuildGEP(builder, exit_value, (LLVMValueRef[]){i32_zero, i32_one},  2, NULL);
  // And store the actual values in them
  LLVMBuildStore(builder, status_value, status_ptr);
  LLVMBuildStore(builder, dest_value,   dest_ptr);
  // Then dereference the whole struct so we can return it
  LLVMBuildRetVoid(builder);
}


LLVMBasicBlockRef hvm_jit_build_bailout_block(hvm_vm *vm, LLVMBuilderRef builder, LLVMValueRef parent_func, LLVMValueRef exit_value, void *void_context, uint64_t ip) {
  // Liven up the type
  struct hvm_jit_compile_context *context = void_context;
  hvm_compile_bundle *bundle = context->bundle;

  // Create the basic block for our bailout code
  LLVMBasicBlockRef basic_block = LLVMAppendBasicBlockInContext(hvm_shared_llvm_context, parent_func, NULL);
  LLVMPositionBuilderAtEnd(builder, basic_block);

  // Get the pointer to the VM registers
  hvm_obj_ref **general_regs = vm->general_regs;
  // Then convert that to an LLVM pointer
  LLVMValueRef general_regs_ptr = LLVMConstInt(pointer_type, (unsigned long long)general_regs, false);

  // TODO: Track writers to general registers so that we know which registers
  //       need copying (instead of wasting time copying all of them)

  // Loop over each computed general reg value and copy that into the VM's
  // general regs
  for(byte i = 0; i < HVM_GENERAL_REGISTERS; i++) {
    LLVMValueRef value_ptr = context->general_regs[i];
    if(value_ptr == NULL) {
      // Don't need to worry about copying NULLs
      continue;
    }
    LLVMValueRef value = hvm_jit_load_general_reg_value(context, builder, i);
    LLVMValueRef idx_val = LLVMConstInt(int32_type, i, true);
    // Get the pointer to the item in the pointer array
    LLVMValueRef reg_ptr = LLVMBuildGEP(builder, general_regs_ptr, (LLVMValueRef[]){idx_val}, 1, NULL);
    // Now actually copy the value into the register
    LLVMBuildStore(builder, value, reg_ptr);
  }

  // Get the function to set a local in the VM frame
  LLVMValueRef func = hvm_jit_set_local_llvm_value(bundle);
  // Make a pointer to the frame
  LLVMValueRef frame_ptr = LLVMConstInt(int64_type, (unsigned long long)(bundle->frame), false);
  frame_ptr = LLVMBuildIntToPtr(builder, frame_ptr, pointer_type, "frame");
  hvm_obj_struct *locals = context->locals;
  // Iterate through the slots in the locals dictionary
  for(unsigned int i = 0; i < locals->heap_length; i++) {
    hvm_obj_struct_heap_pair *pair = locals->heap[i];
    hvm_symbol_id sym = pair->id;
    void *slot        = pair->obj;
    // Load the object ref from the value
    char *symbol_name = hvm_desymbolicate(context->vm->symbols, sym);
    LLVMValueRef value = hvm_jit_load_slot(builder, slot, symbol_name);
    // Create a value for the symbol ID
    LLVMValueRef value_symbol = LLVMConstInt(int64_type, sym, false);
    // Create the call to set the local
    LLVMValueRef args[] = {frame_ptr, value_symbol, value};
    LLVMBuildCall(builder, func, args, 3, "");
  }

  // TODO: Also copy argument registers!
  // TODO: Copy locals from the special JIT stack slots into a regular frame

  // Build the return of the `hvm_jit_exit` structure-union from the JIT code
  // segment/function.
  hvm_jit_build_bailout_return_to_ip(builder, exit_value, ip);

  return basic_block;
}


hvm_jit_block *hvm_jit_compile_find_or_insert_block(LLVMValueRef parent_func, hvm_compile_bundle *bundle, uint64_t ip) {
  LLVMContextRef context = hvm_shared_llvm_context;
  hvm_jit_block *block = NULL;
  hvm_jit_block *new_block;
  // For building the name of the block
  char name[32];

  if(bundle->blocks_head == NULL) {
    // If there's no blocks whatsoever then create a new one and set it
    // as the head and tail
    block = je_malloc(sizeof(hvm_jit_block));
    block->next = NULL;
    bundle->blocks_head = block;
    bundle->blocks_tail = block;
    goto block_inserted;
  }

  // Start with the first block
  block = bundle->blocks_head;
  while(block != NULL) {
    // Don't duplicate blocks
    if(ip == block->ip) {
      return block;
    }
    hvm_jit_block *next = block->next;

    // If there's no more blocks then create a new one and append it
    if(next == NULL) {
      new_block = je_malloc(sizeof(hvm_jit_block));
      new_block->next = NULL;
      // Set the new one as the next in the list and the tail of the list
      block->next = new_block;
      bundle->blocks_tail = new_block;
      block = new_block;
      goto block_inserted;
    }
    // Check if we need to insert between this block and the next block
    if(block->ip < ip && ip < next->ip) {
      new_block = je_malloc(sizeof(hvm_jit_block));
      // Set up the linkages
      block->next = new_block;
      new_block->next = next;
      block = new_block;
      goto block_inserted;
    }
    block = next;
  }

  block_inserted:
  sprintf(name, "block_0x%08llX", ip);
  block->ip = ip;
  if(block->next == NULL) {
    // If we're at the end then append
    block->basic_block = LLVMAppendBasicBlockInContext(context, parent_func, name);
  } else {
    // Otherwise insert before the next block
    LLVMBasicBlockRef next_basic_block = block->next->basic_block;
    block->basic_block = LLVMInsertBasicBlockInContext(context, next_basic_block, name);
  }
  // Update the count
  bundle->blocks_length += 1;
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

// Forward declarations of some utilities
void hvm_jit_position_builder_at_entry(hvm_call_trace*, struct hvm_jit_compile_context*, LLVMBuilderRef);


// Compilation passes ---------------------------------------------------------

void hvm_jit_compile_pass_identify_blocks(hvm_call_trace *trace, hvm_compile_bundle *bundle) {
  // Sanity guards
  assert(trace->sequence_length > 0);

  uint64_t ip;
  hvm_trace_sequence_item *item;
  hvm_compile_sequence_data *data_item;
  hvm_jit_block *block;

  // Unpack the bundle and get the context
  LLVMContextRef context = hvm_shared_llvm_context;
  // Get our parent function that we're building inside.
  LLVMValueRef   parent_func = bundle->llvm_function;

  char scratch[40];

  // Get the first item and set up a block based off of it for entry
  item = &trace->sequence[0];
  // Set up a block for our entry point
  uint64_t entry_ip = item->head.ip;
  sprintf(scratch, "entry_0x%08llX", entry_ip);
  hvm_jit_block *entry  = je_malloc(sizeof(hvm_jit_block));
  entry->next           = NULL;
  entry->ip             = entry_ip;
  entry->basic_block    = LLVMAppendBasicBlockInContext(context, parent_func, scratch);
  bundle->blocks_head   = entry;
  bundle->blocks_tail   = entry;
  bundle->blocks_length = 1;

  for(unsigned int i = 0; i < trace->sequence_length; i++) {
    item = &trace->sequence[i];
    // Also need to be able to set some stuff on the data item in this pass
    data_item = &bundle->data[i];

    switch(item->head.type) {
      case HVM_TRACE_SEQUENCE_ITEM_IF:
        // Handle the truthy destination
        ip = item->item_if.destination;
        if(hvm_jit_trace_contains_ip(trace, ip)) {
          // Set up a TRUTHINESS block for the IP if it was found
          block = hvm_jit_compile_find_or_insert_block(parent_func, bundle, ip);
          data_item->item_if.truthy_block = block;
        } else {
          // Otherwise set the truthy block to NULL so that we know to insert
          // a bailout
          data_item->item_if.truthy_block = NULL;
        }

        ip = item->head.ip + 10;// 10 bytes for op, register, and destination
        if(hvm_jit_trace_contains_ip(trace, ip)) {
          // Setting up a FALSINESS block
          block = hvm_jit_compile_find_or_insert_block(parent_func, bundle, ip);
          data_item->item_if.falsey_block = block;
        } else {
          // Otherwise set to NULL to indicate we need a bailout
          data_item->item_if.falsey_block = NULL;
        }
        break;

      case HVM_TRACE_SEQUENCE_ITEM_GOTO:
        ip = item->item_goto.destination;
        assert(hvm_jit_trace_contains_ip(trace, ip));// TODO: Generate bailout
        block = hvm_jit_compile_find_or_insert_block(parent_func, bundle, ip);
        data_item->item_goto.destination_block = block;
        break;

      default:
        // Don't do anything for other items
        continue;
    }
  }

  // hvm_jit_block *b = bundle->blocks_head;
  // while(b != NULL) {
  //   printf("block at 0x%llx\n", b->ip);
  //   b = b->next;
  // }
}

void hvm_jit_compile_pass_identify_registers(hvm_call_trace *trace, struct hvm_jit_compile_context *context) {
  // Get the builder out of the bundle
  LLVMBuilderRef builder = context->bundle->llvm_builder;
  // And position it at the start so our stack allocations come first
  hvm_jit_position_builder_at_entry(trace, context, builder);

  unsigned int i;
  // Track the number of times a register is written (if it's less than
  // 2 times then we know it will be constant).
  unsigned int writes[HVM_TOTAL_REGISTERS];
  for(i = 0; i < HVM_TOTAL_REGISTERS; i++) {
    writes[i] = 0;
  }

  for(i = 0; i < trace->sequence_length; i++) {
    hvm_trace_sequence_item *item = &trace->sequence[i];
    byte register_return;

    switch(item->head.type) {
      case HVM_TRACE_SEQUENCE_ITEM_SETSTRING:
      case HVM_TRACE_SEQUENCE_ITEM_SETSYMBOL:
      case HVM_TRACE_SEQUENCE_ITEM_INVOKEPRIMITIVE:
      case HVM_TRACE_SEQUENCE_ITEM_ADD:
      case HVM_TRACE_SEQUENCE_ITEM_EQ:
      case HVM_TRACE_SEQUENCE_ITEM_LT:
      case HVM_TRACE_SEQUENCE_ITEM_GT:
      case HVM_TRACE_SEQUENCE_ITEM_AND:
      case HVM_TRACE_SEQUENCE_ITEM_ARRAYGET:
      case HVM_TRACE_SEQUENCE_ITEM_ARRAYLEN:
      case HVM_TRACE_SEQUENCE_ITEM_MOVE:
      case HVM_TRACE_SEQUENCE_ITEM_LITINTEGER:
      case HVM_TRACE_SEQUENCE_ITEM_GETLOCAL:
        register_return = item->returning.register_return;
        writes[register_return] += 1;
        break;
      default:
        continue;
    }
  }
  // Make a pointer to null
  LLVMValueRef null_ptr = LLVMConstInt(int64_type, (unsigned long long)hvm_const_null, false);
  null_ptr = LLVMBuildIntToPtr(builder, null_ptr, obj_ref_ptr_type, "null");

  for(byte i = 0; i < HVM_TOTAL_REGISTERS; i++) {
    // printf("writes[%d] = %u\n", i, writes[i]);
    // Mark the register as constant if we write to it 1 or less times.
    context->constant_regs[i] = (writes[i] < 2);
    // Also pre-allocate it if it's a general register
    // TODO: Actually track usage
    if(hvm_is_gen_reg(i)) {
      // Pre-allocate the slot for it if it's been used at all
      hvm_jit_store_general_reg_value(context, builder, i, null_ptr);
    }
  }
}

void hvm_jit_position_builder_at_entry(hvm_call_trace *trace, struct hvm_jit_compile_context *context, LLVMBuilderRef builder) {
  hvm_trace_sequence_item *entry;
  hvm_jit_block *entry_block;
  LLVMBasicBlockRef entry_basic_block;
  // Get the entry item and its associated block
  entry             = &trace->sequence[0];
  entry_block       = hvm_jit_get_current_block(context->bundle, entry->head.ip);
  entry_basic_block = entry_block->basic_block;
  LLVMPositionBuilderAtEnd(builder, entry_basic_block);
}

void hvm_jit_compile_pass_emit(hvm_vm *vm, hvm_call_trace *trace, struct hvm_jit_compile_context *context) {
  unsigned int i;
  unsigned int type;
  uint64_t ip;
  hvm_jit_block *jit_block;
  hvm_obj_ref *ref;
  hvm_compile_value *cv;
  // 64 bytes to play with for making strings to pass to LLVM
  char scratch[64];
  // Function-pointer-as-value
  LLVMValueRef func;

  hvm_compile_bundle *bundle      = context->bundle;
  hvm_compile_sequence_data *data = bundle->data;
  // hvm_obj_struct *locals       = context->locals;
  LLVMBuilderRef builder          = bundle->llvm_builder;

  hvm_jit_position_builder_at_entry(trace, context, builder);

  // Pre-compute a pointer to our VM instance
  LLVMValueRef value_vm_ptr_int   = LLVMConstInt(int64_type, (unsigned long long)vm, false);
  const LLVMValueRef value_vm_ptr = LLVMBuildIntToPtr(builder, value_vm_ptr_int, pointer_type, "vm");

  // Get the top-level basic block and its parent function
  LLVMValueRef   parent_func = bundle->llvm_function;
  // Extract our pointer to `hvm_jit_exit` struct from the function
  // parameters so we can use it later.
  LLVMValueRef   exit_value  = LLVMGetParam(parent_func, 0);
  // Get the pointer to the array of arg registers
  LLVMValueRef   param_regs  = LLVMGetParam(parent_func, 1);

  for(i = 0; i < trace->sequence_length; i++) {
    hvm_compile_sequence_data *data_item  = &data[i];
    hvm_trace_sequence_item   *trace_item = &trace->sequence[i];

    // uint64_t current_ip = trace_item->head.ip;

    // TODO: Refactor this to be faster!
    hvm_jit_block    *current_block       = hvm_jit_get_current_block(bundle, trace_item->head.ip);
    LLVMBasicBlockRef current_basic_block = current_block->basic_block;
    // Make sure our builder is in the right place
    LLVMPositionBuilderAtEnd(builder, current_basic_block);

    #define NEW_COMPILE_VALUE() je_malloc(sizeof(hvm_compile_value))
    #define STORE(COMPILE_VALUE, LLVM_VALUE) \
      hvm_jit_store_value(context, COMPILE_VALUE); \
      hvm_jit_store_reg_value(context, builder, COMPILE_VALUE->reg, LLVM_VALUE);

    #define DATA_ITEM_TYPE data_item->head.type

    // Do stuff with the item based upon its type.
    switch(trace_item->head.type) {
      case HVM_TRACE_SEQUENCE_ITEM_MOVE:
        DATA_ITEM_TYPE = HVM_COMPILE_DATA_MOVE;
        {
          byte reg_source = trace_item->move.register_source;
          byte reg_return = trace_item->move.register_return;
          // fprintf(stderr, "0x%08llX move %d -> %d\n", trace_item->head.ip, reg_source, reg);
          // Value read from general register or argument register
          LLVMValueRef value;
          if(hvm_is_gen_reg(reg_source)) {
            // Fetch the value from one and put it in the other
            value = hvm_jit_load_general_reg_value(context, builder, reg_source);
          } else if(hvm_is_param_reg(reg_source)) {
            // Extract it from the argument registers array
            unsigned int idx = reg_source - 146;
            // Fetch the parameter pointer from the parameter registers array
            value = LLVMBuildExtractValue(builder, param_regs, idx, "param");
            // Cast it from a simple *i8 pointer to a object reference pointer
            value = LLVMBuildPointerCast(builder, value, obj_ref_ptr_type, "param_obj_ref");
          } else {
            fprintf(stderr, "Can't handle register %d\n", reg_source);
            // Can't handle other register types yet
            assert(false);
          }
          cv = hvm_compile_value_new(HVM_UNKNOWN_TYPE, reg_return);
          data_item->move.register_return = reg_return;
          data_item->move.value = cv;
          // hvm_jit_store_reg_value(context, builder, reg, value);
          STORE(cv, value);
        }
        break;

      case HVM_TRACE_SEQUENCE_ITEM_SETSTRING:
        DATA_ITEM_TYPE = HVM_COMPILE_DATA_SETSTRING;
        {
          byte reg = trace_item->setstring.register_return;
          // Get the object reference from the constant pool
          ref = hvm_const_pool_get_const(&vm->const_pool, trace_item->setstring.constant);
          // Convert it to a pointer
          LLVMValueRef value = LLVMConstInt(int64_type, (unsigned long long)ref, false);
          value = LLVMBuildIntToPtr(builder, value, obj_ref_ptr_type, "string");
          cv = hvm_compile_value_new(HVM_STRING, reg);
          cv->constant = true;
          cv->constant_object = ref;
          STORE(cv, value);
        }
        break;

      case HVM_TRACE_SEQUENCE_ITEM_SETSYMBOL:
        DATA_ITEM_TYPE = HVM_COMPILE_DATA_SETSYMBOL;
        {
          byte reg = trace_item->setsymbol.register_return;
          // Copy over our basic information from the trace item
          data_item->setsymbol.register_return = reg;
          data_item->setsymbol.constant = trace_item->setsymbol.constant;
          // Also compile our symbol as a LLVM value
          ref = hvm_const_pool_get_const(&vm->const_pool, data_item->setsymbol.constant);
          // Integer constant wants an `unsigned long long`.
          LLVMValueRef value = LLVMConstInt(int64_type, (unsigned long long)ref, false);
          value = LLVMBuildIntToPtr(builder, value, obj_ref_ptr_type, "symbol");
          // Save our new value into the data item.
          data_item->setsymbol.value = value;
          cv = hvm_compile_value_new(HVM_SYMBOL, reg);
          cv->constant = true;
          cv->constant_object = ref;
          // TODO: Call a VM function to check this
          STORE(cv, value);
        }
        break;

      case HVM_TRACE_SEQUENCE_ITEM_ARRAYGET:
        DATA_ITEM_TYPE = HVM_COMPILE_DATA_ARRAYGET;
        {
          byte reg_array, reg_index;
          LLVMValueRef value_array, value_index, value_returned;
          // Getting the pointer value to the array
          reg_array   = trace_item->arrayget.register_array;
          value_array = hvm_jit_load_general_reg_value(context, builder, reg_array);
          // Getting the index value
          reg_index   = trace_item->arrayget.register_index;
          value_index = hvm_jit_load_general_reg_value(context, builder, reg_index);
          // Get the function as a LLVM value we can work with
          func = hvm_jit_obj_array_get_llvm_value(bundle);
          LLVMValueRef arrayget_args[2] = {value_array, value_index};
          // Build the function call
          value_returned = LLVMBuildCall(builder, func, arrayget_args, 2, "result");
          // Save the return value
          byte reg = trace_item->arrayget.register_return;
          cv       = hvm_compile_value_new(HVM_UNKNOWN_TYPE, reg);
          STORE(cv, value_returned);
        }
        break;

      case HVM_TRACE_SEQUENCE_ITEM_EQ:
        data_item->head.type = HVM_COMPILE_DATA_EQ;
        {
          byte reg, reg1, reg2;
          LLVMValueRef value1, value2, value;
          // Unpack register and load
          reg    = trace_item->eq.register_return;
          reg1   = trace_item->eq.register_operand1;
          reg2   = trace_item->eq.register_operand2;
          value1 = hvm_jit_load_general_reg_value(context, builder, reg1);
          value2 = hvm_jit_load_general_reg_value(context, builder, reg2);
          // fprintf(stderr, "value1: $%d %s\n", reg1, LLVMPrintTypeToString(LLVMTypeOf(value1)));
          // fprintf(stderr, "value2: $%d %s\n", reg2, LLVMPrintTypeToString(LLVMTypeOf(value2)));
          // Build the `hvm_obj_int_eq` call
          func  = hvm_jit_obj_int_eq_llvm_value(bundle);
          LLVMValueRef int_eq_args[3] = {value_vm_ptr, value1, value2};
          value = LLVMBuildCall(builder, func, int_eq_args, 3, "equal");
          // TODO: Check if return is NULL and raise proper exception
          cv = hvm_compile_value_new(HVM_INTEGER, reg);
          STORE(cv, value);
        }
        break;

      case HVM_TRACE_SEQUENCE_ITEM_AND:
        DATA_ITEM_TYPE = HVM_COMPILE_DATA_AND;
        {
          LLVMValueRef value, value1, value2, value_returned, data_ptr;
          byte reg, reg1, reg2;
          // Unpack register and build loads from JIT register slots
          reg    = trace_item->eq.register_return;
          reg1   = trace_item->eq.register_operand1;
          reg2   = trace_item->eq.register_operand2;
          value1 = hvm_jit_load_general_reg_value(context, builder, reg1);
          value2 = hvm_jit_load_general_reg_value(context, builder, reg2);
          // Get the is-truthy call
          func = hvm_jit_obj_is_truthy_llvm_value(bundle);
          // Transform value1 and value2 into booleans via `hvm_obj_is_truthy`
          value1 = LLVMBuildCall(builder, func, (LLVMValueRef[]){value1}, 1, "is_truthy");
          value2 = LLVMBuildCall(builder, func, (LLVMValueRef[]){value2}, 1, "is_truthy");
          // Then do an and comparison of those two
          sprintf(scratch, "value = $%-3d && $%-3d", reg1, reg2);
          value = LLVMBuildAnd(builder, value1, value2, scratch);
          // Convert our value to an i64.
          value = LLVMBuildIntCast(builder, value, int64_type, "value");
          // And build an integer with that value
          func = hvm_jit_new_obj_int_llvm_value(bundle);
          // Going to get a pointer and cast it properly
          LLVMValueRef new_obj_int_args[1] = {value_vm_ptr};
          value_returned = LLVMBuildCall(builder, func, new_obj_int_args, 1, "obj_ref_int");
          value_returned = LLVMBuildPointerCast(builder, value_returned, obj_ref_ptr_type, "value_returned");
          // Then get the pointer to the data property and set it (first 0 index
          // is to get the first value pointed at, the second 0 index is to get
          // the first item in the struct).
          data_ptr = LLVMBuildGEP(builder, value_returned, (LLVMValueRef[]){i32_zero, i32_one}, 2, "data_ptr");
          // Convert it to the proper 64-bit integer pointer
          data_ptr = LLVMBuildPointerCast(builder, data_ptr, int64_pointer_type, "data_ptr");
          LLVMBuildStore(builder, value, data_ptr);
          // Slow comparison path:
          // func           = hvm_jit_obj_cmp_and_llvm_value(bundle);
          // value_returned = LLVMBuildCall(builder, func, (LLVMValueRef[]){value1, value2}, 2, "and");
          // hvm_jit_store_reg_value(context, builder, reg, value_returned);
          cv = hvm_compile_value_new(HVM_INTEGER, reg);
          STORE(cv, value_returned);
        }
        break;

      case HVM_TRACE_SEQUENCE_ITEM_ARRAYSET:
        DATA_ITEM_TYPE = HVM_COMPILE_DATA_ARRAYSET;
        {
          // Get the values we need for the array-set operation
          byte         reg_array   = trace_item->arrayset.register_array;
          byte         reg_index   = trace_item->arrayset.register_index;
          byte         reg_value   = trace_item->arrayset.register_value;
          LLVMValueRef value_array = hvm_jit_load_general_reg_value(context, builder, reg_array);
          LLVMValueRef value_index = hvm_jit_load_general_reg_value(context, builder, reg_index);
          LLVMValueRef value       = hvm_jit_load_general_reg_value(context, builder, reg_value);
          // Get the array-set function
          func = hvm_jit_obj_array_set_llvm_value(bundle);
          LLVMValueRef arrayset_args[3] = {value_array, value_index, value};
          // Build the function call with the function value and arguments
          LLVMBuildCall(builder, func, arrayset_args, 3, "");
        }
        break;

      case HVM_TRACE_SEQUENCE_ITEM_ARRAYLEN:
        DATA_ITEM_TYPE = HVM_COMPILE_DATA_ARRAYLEN;
        {
          byte reg, reg_array;
          LLVMValueRef value_returned;
          // Unpack registers
          reg       = trace_item->arraylen.register_return;
          reg_array = trace_item->arraylen.register_array;
          // Source array that we'll be getting the length of
          LLVMValueRef value_array = hvm_jit_load_general_reg_value(context, builder, reg_array);
          // Get the array-length function
          func = hvm_jit_obj_array_len_llvm_value(bundle);
          LLVMValueRef arraylen_args[2] = {value_vm_ptr, value_array};
          // Then build the function call
          value_returned = LLVMBuildCall(builder, func, arraylen_args, 2, "arraylen");
          // JIT_SAVE_DATA_ITEM_AND_VALUE(reg, data_item, value_returned);
          // hvm_jit_store_reg_value(context, builder, reg, value_returned);
          cv = hvm_compile_value_new(HVM_INTEGER, reg);
          STORE(cv, value_returned);
        }
        break;

      case HVM_TRACE_SEQUENCE_ITEM_INVOKEPRIMITIVE:
        DATA_ITEM_TYPE = HVM_COMPILE_DATA_INVOKEPRIMITIVE;
        {
          LLVMValueRef value_returned;
          byte reg = trace_item->invokeprimitive.register_return;
          // Get the source value information
          byte reg_symbol = trace_item->invokeprimitive.register_symbol;
          LLVMValueRef value_symbol = hvm_jit_load_general_reg_value(context, builder, reg_symbol);
          assert(value_symbol != NULL);
          // Build and run the call to copy the registers
          func = hvm_jit_vm_copy_regs_llvm_value(bundle);
          LLVMBuildCall(builder, func, (LLVMValueRef[]){value_vm_ptr}, 1, "");
          // Build the call to `hvm_vm_call_primitive`.
          func = hvm_jit_vm_call_primitive_llvm_value(bundle);
          LLVMValueRef invokeprimitive_args[2] = {value_vm_ptr, value_symbol};
          value_returned = LLVMBuildCall(builder, func, invokeprimitive_args, 2, "result");
          // hvm_jit_store_reg_value(context, builder, reg, value_returned);
          cv = hvm_compile_value_new(HVM_UNKNOWN_TYPE, reg);
          STORE(cv, value_returned);
        }
        break;

      case HVM_TRACE_SEQUENCE_ITEM_ADD:
        DATA_ITEM_TYPE = HVM_COMPILE_DATA_ADD;
        {
          byte reg, reg1, reg2;
          LLVMValueRef value1, value2, value_returned;
          // Unpack registers
          reg  = trace_item->add.register_return;
          reg1 = trace_item->add.register_operand1;
          reg2 = trace_item->add.register_operand2;
          // Fetch the meta-data about those values
          hvm_compile_value *ov1 = hvm_jit_get_value(context, reg1);
          hvm_compile_value *ov2 = hvm_jit_get_value(context, reg2);
          if(ov1->type == HVM_INTEGER && ov2->type == HVM_INTEGER) {
            // printf("using direct addition code path at 0x%08llX\n", trace_item->head.ip);
            value_returned = hvm_jit_obj_int_add_direct(context, builder, value_vm_ptr, ov1, ov2, reg1, reg2);
          } else {
            // Get the source values for the operation
            value1 = hvm_jit_load_general_reg_value(context, builder, reg1);
            value2 = hvm_jit_load_general_reg_value(context, builder, reg2);
            // TODO: Currently this will fail if given non-integer values.
            func = hvm_jit_obj_int_add_llvm_value(bundle);
            LLVMValueRef add_args[3] = {value_vm_ptr, value1, value2};
            value_returned = LLVMBuildCall(builder, func, add_args, 3, "added");
          }
          cv = hvm_compile_value_new(HVM_INTEGER, reg);
          STORE(cv, value_returned);
        }
        break;

      case HVM_TRACE_SEQUENCE_ITEM_GOTO:
        DATA_ITEM_TYPE = HVM_COMPILE_DATA_GOTO;
        // Look up the block we need to go to.
        jit_block = data_item->item_goto.destination_block;
        // Build the branch instruction to this block
        LLVMBuildBr(builder, jit_block->basic_block);
        continue;// Skip continuation checks

      case HVM_TRACE_SEQUENCE_ITEM_GT:
        // Extract the registers and values
        {
          byte reg1, reg2, reg_result;
          LLVMValueRef value1, value2, value_returned;
          // Unpack and load
          reg_result = trace_item->add.register_return;
          reg1       = trace_item->add.register_operand1;
          reg2       = trace_item->add.register_operand2;
          value1     = hvm_jit_load_general_reg_value(context, builder, reg1);
          value2     = hvm_jit_load_general_reg_value(context, builder, reg2);
          // Fetch the type for determining which comparison function to use
          type = trace_item->head.type;
          // Save the operation type and lookup the comparison function
          if(type == HVM_TRACE_SEQUENCE_ITEM_GT) {
            data_item->head.type = HVM_COMPILE_DATA_GT;
            func = hvm_jit_obj_int_gt_llvm_value(bundle);
          } else {
            assert(false);
          }
          // Call our comparator and store the result
          LLVMValueRef comparison_args[3] = {value_vm_ptr, value1, value2};
          // sprintf(scratch, "$%-3d = $%-3d > $%-3d", reg_result, reg1, reg2);
          value_returned = LLVMBuildCall(builder, func, comparison_args, 3, "gt");
          // TODO: Check for exception set by primitive or NULL return from it
          // JIT_SAVE_DATA_ITEM_AND_VALUE(reg_result, data_item, value_returned);
          // hvm_jit_store_reg_value(context, builder, reg, value_returned);
          cv = hvm_compile_value_new(HVM_INTEGER, reg_result);
          STORE(cv, value_returned);
        }
        break;

      case HVM_TRACE_SEQUENCE_ITEM_IF:
        DATA_ITEM_TYPE = HVM_COMPILE_DATA_IF;
        // Log with the scratch
        // sprintf(scratch, "if %p ? ->%p : ->%p", value1, data_item->item_if.truthy_block, data_item->item_if.falsey_block);
        // hvm_jit_llvm_print_string(bundle, builder, scratch);
        {
          // Building our comparison:
          //   falsey = (val->type == HVM_NULL || (val->type == HVM_INTEGER && val->data.i64 == 0))
          //   truthy = !falsey
          //   if truthy then branch
          //
          // Extract the value we'll be testing and cast it to an hvm_obj_ref
          // type in the LLVM IR.
          byte         reg1   = trace_item->item_if.register_value;
          LLVMValueRef value1 = hvm_jit_load_general_reg_value(context, builder, reg1);

          // Slow thruthy path:
          // func   = hvm_jit_obj_is_truthy_llvm_value(bundle);
          // LLVMValueRef truthy_args[1] = {value1};
          // LLVMValueRef truthy         = LLVMBuildCall(builder, func, truthy_args, 1, "truthy");
          // // Truncate down to a int1/bool
          // truthy = LLVMBuildTrunc(builder, truthy, int1_type, "");

          // Fast truthy path:
          // Expects `hvm_obj_ref` pointer and should return a bool LLVM value ref
          LLVMValueRef falsey = hvm_jit_compile_value_is_falsey(builder, value1);
          // Invert for our truthy test
          LLVMValueRef truthy = LLVMBuildNot(builder, falsey, "truthy");

          // Get the TRUTHY block to branch to or set up a bailout
          LLVMBasicBlockRef truthy_block;
          if(data_item->item_if.truthy_block != NULL) {
            truthy_block = data_item->item_if.truthy_block->basic_block;
          } else {
            ip = trace_item->item_if.destination;
            truthy_block = hvm_jit_build_bailout_block(vm, builder, parent_func, exit_value, context, ip);
          }
          // Same for the FALSEY block
          LLVMBasicBlockRef falsey_block;
          if(data_item->item_if.falsey_block != NULL) {
            falsey_block = data_item->item_if.falsey_block->basic_block;
          } else {
            // Falsey just continues past the instruction
            ip = trace_item->head.ip + 10;
            falsey_block = hvm_jit_build_bailout_block(vm, builder, parent_func, exit_value, context, ip);
          }
          // And finally actually do the branch with those blocks
          LLVMBuildCondBr(builder, truthy, truthy_block, falsey_block);
        }
        continue;// Skip continuation checks

      case HVM_TRACE_SEQUENCE_ITEM_RETURN:
        DATA_ITEM_TYPE = HVM_COMPILE_DATA_RETURN;
        {
          byte reg;
          LLVMTypeRef  status_type, er_ptr_type;
          LLVMValueRef status_value, exit_return, status_ptr, value_ptr, value;
          // Unpack and load
          reg = trace_item->item_return.register_return;
          value = hvm_jit_load_general_reg_value(context, builder, reg);
          // Set up the status
          status_type  = LLVMIntType(sizeof(hvm_jit_exit_status) * 8);
          status_value = LLVMConstInt(status_type, HVM_JIT_EXIT_RETURN, false);
          // Cast exit value to exit return
          er_ptr_type  = LLVMPointerType(hvm_jit_exit_return_llvm_type(), 0);
          exit_return  = LLVMBuildPointerCast(builder, exit_value, er_ptr_type, "exit_return");
          // Pointers into the struct
          status_ptr   = LLVMBuildGEP(builder, exit_return, (LLVMValueRef[]){i32_zero, i32_zero}, 2, "status_ptr");
          value_ptr    = LLVMBuildGEP(builder, exit_return, (LLVMValueRef[]){i32_zero, i32_one},  2, "value_ptr");
          // Set the status and return value into the struct
          LLVMBuildStore(builder, status_value, status_ptr);
          LLVMBuildStore(builder, value, value_ptr);
          LLVMBuildRetVoid(builder);
        }
        continue;

      case HVM_TRACE_SEQUENCE_ITEM_LITINTEGER:
        DATA_ITEM_TYPE = HVM_COMPILE_DATA_LITINTEGER;
        {
          LLVMValueRef value;
          hvm_obj_ref *ref;
          byte reg = trace_item->litinteger.register_return;
          // Create a new object reference and store the literal value in it
          ref = hvm_new_obj_int(vm);
          // Mark it as a constant to be exempt from GC.
          ref->flags = ref->flags | HVM_OBJ_FLAG_CONSTANT;
          // TODO: Add it to the object space so that the VM and GC will still
          //       know about it.
          ref->data.i64 = trace_item->litinteger.literal_value;
          // Convert the reference to a pointer
          value = LLVMConstInt(int64_type, (unsigned long long)ref, false);
          value = LLVMBuildIntToPtr(builder, value, obj_ref_ptr_type, "integer");
          // Save that value into the data item
          data_item->litinteger.value = value;
          data_item->litinteger.register_return = reg;
          cv = hvm_compile_value_new(HVM_INTEGER, reg);
          cv->constant = true;
          cv->constant_object = ref;
          // Then save the value into the "registers"
          STORE(cv, value);
        }
        break;

      case HVM_TRACE_SEQUENCE_ITEM_SETLOCAL:
        DATA_ITEM_TYPE = HVM_COMPILE_DATA_SETLOCAL;
        {
          // Register that the symbol object is going to be in
          byte reg_value = trace_item->setlocal.register_value;
          // Get register of the symbol and the compile value
          byte reg_symbol = trace_item->setlocal.register_symbol;
          hvm_compile_value *cv_sym = hvm_jit_get_value(context, reg_symbol);
          // Must be constant for us to use fast stack slot storage
          assert(cv_sym->type == HVM_SYMBOL && cv_sym->constant);
          // Look up symbol ID from the trace
          // hvm_symbol_id symbol_id = trace_item->setlocal.symbol_value;
          // Read the value to be written into the slot
          LLVMValueRef value = hvm_jit_load_general_reg_value(context, builder, reg_value);
          // Look up and write to the slot
          // void *slot = hvm_obj_struct_internal_get(locals, symbol_id);
          LLVMValueRef slot = data_item->setlocal.slot;
          assert(slot != NULL);
          hvm_jit_store_slot(builder, (LLVMValueRef)slot, value, "");
        }
        /*
        // Pull out the symbol ID from the value in reg_symbol
        value_symbol = hvm_jit_load_general_reg_value(context, builder, reg_symbol);
        value_symbol = hvm_jit_load_symbol_id_from_obj_ref_value(builder, value_symbol);
        // Get the value to be written
        value = hvm_jit_load_general_reg_value(context, builder, reg_value);
        // Create a pointer to the `hvm_frame`
        frame_ptr = LLVMConstInt(int64_type, (unsigned long long)(bundle->frame), false);
        frame_ptr = LLVMBuildIntToPtr(builder, frame_ptr, pointer_type, "frame");
        // Build the call to write to the frame
        func = hvm_jit_set_local_llvm_value(bundle);
        LLVMBuildCall(builder, func, (LLVMValueRef[]){frame_ptr, value_symbol, value}, 3, "");
        */
        break;

      case HVM_TRACE_SEQUENCE_ITEM_GETLOCAL:
        DATA_ITEM_TYPE = HVM_COMPILE_DATA_GETLOCAL;
        {
          LLVMValueRef value;
          // Where the local variable is going to end up
          byte reg_result = trace_item->getlocal.register_return;
          // Get the symbol ID for the local
          // hvm_symbol_id symbol_id = trace_item->getlocal.symbol_value;
          // Get the slot for that local from the locals slots structure
          // void *slot = hvm_obj_struct_internal_get(locals, symbol_id);
          LLVMValueRef slot = data_item->getlocal.slot;
          assert(slot != NULL);
          // Load the object ref out of that slot
          value = hvm_jit_load_slot(builder, slot, "");

          // The below is a buggy attempt at an optimized code path:
          /*
          // Get the register containing the symbol then get the compile value
          byte reg_symbol = trace_item->getlocal.register_symbol;
          hvm_compile_value *cv_sym = hvm_jit_get_value(context, reg_symbol);

          if(cv_sym->type == HVM_SYMBOL && cv_sym->constant) {
            // If we know it's constant then we can use the fast code path
            // printf("using direct slot code path at 0x%08llX\n", trace_item->head.ip);
            // Look up the symbol ID from the trace
            hvm_symbol_id symbol_id = trace_item->getlocal.symbol_value;
            void *slot = hvm_obj_struct_internal_get(locals, symbol_id);
            assert(slot != NULL);
            value = hvm_jit_load_slot(builder, slot, "slot_local");
          } else {
            // Otherwise we have to use the slow path
            LLVMValueRef value_symbol = hvm_jit_load_general_reg_value(context, builder, reg_symbol);
            // Get the symbol ID from the object-ref
            LLVMValueRef value_symbol_id = hvm_jit_load_symbol_id_from_obj_ref_value(builder, value_symbol);
            // Get the pointer to the frame
            LLVMValueRef frame_ptr = LLVMConstInt(int64_type, (unsigned long long)(bundle->frame), false);
            frame_ptr = LLVMBuildIntToPtr(builder, frame_ptr, pointer_type, "frame");
            func      = hvm_jit_get_local_llvm_value(bundle);
            value     = LLVMBuildCall(builder, func, (LLVMValueRef[]){frame_ptr, value_symbol_id}, 2, "local");
          }
          */
          cv = hvm_compile_value_new(HVM_UNKNOWN_TYPE, reg_result);
          STORE(cv, value);
        }
        /*
        // Get the symbol ID
        value_symbol = hvm_jit_load_general_reg_value(context, builder, reg_symbol);
        value_symbol = hvm_jit_load_symbol_id_from_obj_ref_value(builder, value_symbol);
        // Get the frame and read the local from it
        frame_ptr = LLVMConstInt(int64_type, (unsigned long long)(bundle->frame), false);
        frame_ptr = LLVMBuildIntToPtr(builder, frame_ptr, pointer_type, "frame");
        func      = hvm_jit_get_local_llvm_value(bundle);
        value     = LLVMBuildCall(builder, func, (LLVMValueRef[]){frame_ptr, value_symbol}, 2, "get_local");
        */
        break;

      default:
        type = trace_item->head.type;
        fprintf(stderr, "jit-compiler: Don't know what to do with item type %d\n", type);
        assert(false);
        break;
    }//switch

    // See if we need to check for a next-block continuation
    if((i + 1) < trace->sequence_length) {
      uint64_t          next_ip    = (&trace->sequence[i + 1])->head.ip;
      hvm_jit_block    *next_block = hvm_jit_get_current_block(bundle, next_ip);
      LLVMBasicBlockRef next_basic_block = next_block->basic_block;
      // See if we're at the end of our current block
      if(current_basic_block != next_basic_block) {
        // If we are then set up a continuation to the next block
        LLVMBuildBr(builder, next_basic_block);
      }
    }//if

  }//for
}

void hvm_jit_compile_pass_identify_locals(hvm_call_trace *trace, struct hvm_jit_compile_context *context) {
  hvm_compile_sequence_data *data = context->bundle->data;
  LLVMBuilderRef builder = context->bundle->llvm_builder;
  // Set up a structure to store all of our LLVMValueRefs; each local will
  // get its own LLVMValueRef slot
  hvm_obj_struct *locals = hvm_new_obj_struct();
  // Register the local is being read from/written to
  byte reg_return, reg_value;
  hvm_symbol_id symbol_id;
  void *slot;
  hvm_trace_sequence_item *item;

  // Position the builder at the entry; these allocations need to happen at
  // the beginning of the function so that the slots will be available for
  // subsequent instructions in the function body
  hvm_jit_position_builder_at_entry(trace, context, builder);

  unsigned int i;
  for(i = 0; i < trace->sequence_length; i++) {
    item = &trace->sequence[i];
    hvm_compile_sequence_data *data_item = &data[i];

    switch(item->head.type) {
      case HVM_TRACE_SEQUENCE_ITEM_GETLOCAL:
        reg_return = item->getlocal.register_return;
        symbol_id  = item->getlocal.symbol_value;
        // Fetching the hvm_obj_ref* pointer but casting it as a void since
        // it's really just an LLVMValueRef slot pointer
        slot = hvm_obj_struct_internal_get(locals, symbol_id);
        assert(slot != NULL);
        // Convert it to a LLVMValueRef to add it to the `data_item`
        data_item->getlocal.slot = (LLVMValueRef)slot;
        break;
      case HVM_TRACE_SEQUENCE_ITEM_SETLOCAL:
        // Register that is being read
        reg_value = item->setlocal.register_value;
        symbol_id = item->setlocal.symbol_value;
        // Check if the local's slot has already been allocated
        slot = hvm_obj_struct_internal_get(locals, symbol_id);
        if(slot == NULL) {
          char scratch[80];// FIXME: Possible overflow here
          char *symbol_name = hvm_desymbolicate(context->vm->symbols, symbol_id);
          sprintf(scratch, "local:%s", symbol_name);
          // Allocate the slot and add it to the structure dictionary
          slot = LLVMBuildAlloca(builder, obj_ref_ptr_type, scratch);
          hvm_obj_struct_internal_set(locals, symbol_id, slot);
        }
        data_item->setlocal.slot = (LLVMValueRef)slot;
        break;
      default:
        continue;
    }
  }
  context->locals = locals;
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

// Compilation public API -----------------------------------------------------

void hvm_jit_compile_trace(hvm_vm *vm, hvm_call_trace *trace) {
  // Make sure our LLVM context, module, engine, etc. are available
  hvm_jit_setup_llvm();
  LLVMContextRef         context = hvm_shared_llvm_context;
  LLVMModuleRef          module  = hvm_shared_llvm_module;
  LLVMExecutionEngineRef engine  = hvm_shared_llvm_engine;
  // Make sure our constants and such are already defined
  hvm_jit_define_constants();

  // Build the name for our function
  char *function_name = je_malloc(sizeof(char) * 64);
  function_name[0]    = '\0';
  sprintf(function_name, "hvm_jit_function_%p", trace);

  LLVMTypeRef ptr_array_type = LLVMArrayType(pointer_type, HVM_ARGUMENT_REGISTERS);

  LLVMTypeRef  function_args[] = {
    pointer_type,// hvm_jit_exit*
    ptr_array_type // hvm_obj_ref*[]
  };
  LLVMTypeRef  function_type   = LLVMFunctionType(void_type, function_args, 2, false);
  LLVMValueRef function        = LLVMAddFunction(module, function_name, function_type);
  // Builder that we'll write the instructions from our trace into
  LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);

  // Allocate an array of data items for each item in the trace
  hvm_compile_sequence_data *data = je_calloc(trace->sequence_length, sizeof(hvm_compile_sequence_data));
  // Establish a bundle for all of our stuff related to this compilation.
  hvm_compile_bundle bundle = {
    .frame = hvm_new_frame(),
    .data  = data,
    .llvm_module   = module,
    .llvm_builder  = builder,
    .llvm_engine   = engine,
    .llvm_function = function,
    .blocks_head   = NULL,
    .blocks_tail   = NULL,
    .blocks_length = 0
  };

  // Eventually going to run this as a hopefully-two-pass compilation. For now
  // though it's going to be multi-pass.

  // Rearrange the trace to be a linear sequence of ordered IPs
  hvm_jit_sort_trace(trace);


  // Initialize array pointer containers to NULL
  LLVMValueRef general_reg_boxes[HVM_GENERAL_REGISTERS];
  for(unsigned int i = 0; i < HVM_GENERAL_REGISTERS; i++) {
    general_reg_boxes[i] = NULL;
  }
  // Wrapped values read and written into registers during the trace
  hvm_compile_value *wrapped_values[HVM_TOTAL_REGISTERS];
  // Registers marked as constant
  bool constant_registers[HVM_TOTAL_REGISTERS];
  // Setting up the context
  struct hvm_jit_compile_context compile_context = {
    .bundle        = &bundle,
    .general_regs  = general_reg_boxes,
    .constant_regs = constant_registers,
    .vm            = vm,
    .values        = wrapped_values
  };

  // Break our sequence into blocks based upon possible destinations of
  // jumps/ifs/gotos/etc.
  hvm_jit_compile_pass_identify_blocks(trace, &bundle);

  // Identify potential guard points to be checked before/during/after
  // execution.

  // Identify registers with constant values that we can optimize.
  hvm_jit_compile_pass_identify_registers(trace, &compile_context);

  // Identify and extract gets/sets of globals and locals into dedicated
  // in-out pointers arguments to the block so that they can be passed
  // by the VM into the block at call time.
  hvm_jit_compile_pass_identify_locals(trace, &compile_context);

  // Resolve register references in instructions into concrete IR value
  // references and build the instruction sequence.
  hvm_jit_compile_pass_emit(vm, trace, &compile_context);

  // Now let's run the LLVM passes on the function
  LLVMRunFunctionPassManager(hvm_shared_llvm_pass_manager, function);
  // Verify and abort if it's invalid
  char *err = NULL;
  LLVMVerifyModule(module, LLVMAbortProcessAction, &err);
  LLVMDisposeMessage(err);

  // LLVMDumpModule(module);
  // exit(1);

  // Save the compiled function
  trace->compiled_function = function;

  je_free(data);
}

hvm_jit_exit *hvm_jit_run_compiled_trace(hvm_vm *vm, hvm_call_trace *trace) {
  LLVMExecutionEngineRef engine = hvm_shared_llvm_engine;
  LLVMValueRef function         = trace->compiled_function;
  // Set up the memory for our exit information.
  hvm_jit_exit *result = je_malloc(sizeof(hvm_jit_exit));
  // Get a pointer to the JIT-compiled native code
  void *vfp = LLVMGetPointerToGlobal(engine, function);
  // Cast it to the correct function pointer type and call the code
  hvm_jit_native_function fp = (hvm_jit_native_function)vfp;
  fp(result, *(vm->param_regs));
  // Return the result
  return result;
}
