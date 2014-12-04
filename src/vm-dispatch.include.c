
// Normal and JIT dispatching hooks
#define EXECUTE_JIT execute_jit

// Undefine EXECUTE, EXCEPTION, and IN_JIT since we'll always be redefining them
#undef EXECUTE
#undef EXCEPTION
#undef IN_JIT

#ifdef JIT_DISPATCH
#define EXECUTE   execute_jit
#define EXCEPTION handle_exception_jit
// Use IN_JIT to add operations to be performed when in the JIT version
// of the dispatcher.
//   eg. IN_JIT(
//     hvm_jit_tracer_annotate(..);
//   )
#define IN_JIT(V) V
#else
#define EXECUTE   execute
#define EXCEPTION handle_exception
#define IN_JIT(V)
#endif


EXECUTE:
  // fprintf(stderr, "top: %p, ip: %llu\n", vm->top, vm->ip);
  // Update the current frame address
  vm->top->current_addr = vm->ip;
  // Fetch the instruction
  instr = vm->program[vm->ip];

#ifdef HVM_VM_DEBUG
  // Debugger breakpoint-checking code goes here
  should_continue = hvm_debug_before_instruction(vm);
  if(!should_continue) {
    // Halt the VM's execution
    fprintf(stderr, "Execution halted by debugger.\n");
    return;
  }
#endif

  IN_JIT(
    // If this is the dispatch loop for JIT tracing...
    assert(vm->is_tracing == 1);
    // Then trace the instruction
    hvm_jit_tracer_before_instruction(vm);
  )

  // Execute the instruction
  switch(instr) {
    case HVM_OP_NOOP:
      // fprintf(stderr, "NOOP\n");
      break;
    case HVM_OP_DIE:
      // fprintf(stderr, "DIE\n");
      goto end;
    case HVM_OP_TAILCALL:// 1B OP | 3B TAG | 8B DEST
      PROCESS_TAG;
      dest = READ_U64(&vm->program[vm->ip + 4]);
      // Copy important bits from parent.
      parent_frame = vm->top;
      uint64_t parent_ret_addr = parent_frame->return_addr;
      byte     parent_ret_reg  = parent_frame->return_register;
      // Overwrite current frame (ie. parent).
      frame = &vm->stack[vm->stack_depth];
      hvm_frame_initialize(frame);
      frame->return_addr     = parent_ret_addr;
      frame->return_register = parent_ret_reg;
      hvm_vm_copy_regs(vm);
      vm->ip = dest;
      goto EXECUTE;
    case HVM_OP_CALL:// 1B OP | 3B TAG | 8B DEST  | 1B REG
      PROCESS_TAG;
      dest = READ_U64(&vm->program[vm->ip + 4]);
      reg  = vm->program[vm->ip + 12];
      vm->stack_depth += 1;
      frame = &vm->stack[vm->stack_depth];
      hvm_frame_initialize(frame);
      frame->return_addr     = vm->ip + 13; // Instruction is 13 bytes long.
      frame->return_register = reg;
      hvm_vm_copy_regs(vm);
      vm->ip = dest;
      vm->top = frame;
      goto EXECUTE;
    case HVM_OP_CALLSYMBOLIC:// 1B OP | 3B TAG | 4B CONST | 1B REG
      PROCESS_TAG;
      const_index = READ_U32(&vm->program[vm->ip + 4]);
      reg         = vm->program[vm->ip + 8];
      // Get the symbol out of the constant table
      key = hvm_vm_get_const(vm, const_index);
      assert(key->type == HVM_SYMBOL);
      sym_id = key->data.u64;
      char *sym_name = hvm_desymbolicate(vm->symbols, sym_id);
      // fprintf(stderr, "debug: %s:0x%08llX has heat %u\n", sym_name, dest, tag.heat);
      // Preserve the caller's tag for tracing stuff below
      caller_tag = &vm->program[vm->ip + 1];
      // Get the destination from the symbol table
      val  = hvm_obj_struct_internal_get(vm->symbol_table, sym_id);
      assert(val->type == HVM_INTERNAL);
      dest = val->data.u64;
      // Then perform the call
      vm->stack_depth += 1;
      frame = &vm->stack[vm->stack_depth];
      hvm_frame_initialize(frame);
      frame->return_addr     = vm->ip + 9;
      frame->return_register = reg;
      hvm_vm_copy_regs(vm);
      vm->ip = dest;
      vm->top = frame;
      // Check if we've reached the heat threshold
      if(tag.heat > HVM_TRACE_THRESHOLD || vm->always_trace) {
        // See if we have a completed trace available to compile and switch to
        if(tag.trace_index > 0) {
          // .trace_index is offset by one so that we can use 0 to mean
          // no-trace-exists.
          trace = vm->traces[tag.trace_index - 1];
          // Guard that the trace really is completed
          assert(trace->complete);
          // If we don't already have a compiled function then compile it
          if(trace->compiled_function == NULL) {
            hvm_jit_compile_trace(vm, trace);
            fprintf(stderr, "compiled trace for %s:0x%08llX\n", sym_name, dest);
          }
          fprintf(stderr, "running compiled trace for %s:0x%08llX\n", sym_name, dest);
          hvm_jit_exit *result = hvm_jit_run_compiled_trace(vm, trace);
          if(result->ret.status == HVM_JIT_EXIT_BAILOUT) {
            vm->ip = result->bailout.destination;
            goto EXECUTE;
          } else {
            assert(vm->stack_depth != 0);
            vm->ip = frame->return_addr;
            vm->stack_depth -= 1;
            vm->top = &vm->stack[vm->stack_depth];
            hvm_vm_register_write(vm, frame->return_register, result->ret.value);
            goto EXECUTE;
          }
        }
        // fprintf(stderr, "subroutine %s:0x%08llX has heat %d\n", sym_name, dest, tag.heat);
        // Check if we need to start tracing
        if(!vm->is_tracing) {
          // If frame is already being traced
          if(frame->trace != NULL) {
            goto EXECUTE_JIT;
          }
          fprintf(stderr, "switching to trace dispatch for %s:0x%08llX\n", sym_name, dest);
          trace = hvm_new_call_trace(vm);
          trace->caller_tag = caller_tag;
          frame->trace = trace;
          vm->is_tracing = 1;
          goto EXECUTE_JIT;
        }
      }
      goto EXECUTE;

    case HVM_OP_INVOKESYMBOLIC:// 1B OP | 3B TAG | 1B REG | 1B REG
      PROCESS_TAG;
      areg = vm->program[vm->ip + 4];
      breg = vm->program[vm->ip + 5];
      key = _hvm_vm_register_read(vm, areg);// This is the symbol we need to look up.
      assert(key->type == HVM_SYMBOL);
      sym_id = key->data.u64;
      // fprintf(stderr, "0x%08llX  ", vm->ip);
      // fprintf(stderr, "sym: %llu -> %s\n", sym_id, hvm_desymbolicate(vm->symbols, sym_id));
      // hvm_obj_print_structure(vm, vm->symbol_table);
      val  = hvm_obj_struct_internal_get(vm->symbol_table, sym_id);
      assert(val->type == HVM_INTERNAL);
      dest = val->data.u64;
      // fprintf(stderr, "CALLSYMBOLIC(0x%08llX, $%d)\n", dest, breg);
      vm->stack_depth += 1;
      frame = &vm->stack[vm->stack_depth];
      hvm_frame_initialize(frame);
      frame->return_addr = vm->ip + 6;// Instruction is 6 bytes long
      frame->return_register = breg;
      hvm_vm_copy_regs(vm);
      vm->ip = dest;
      vm->top = frame;
      goto EXECUTE;
    case HVM_OP_INVOKEADDRESS:// 1B OP | 3B TAG | 1B REG | 1B REG
      PROCESS_TAG;
      reg  = vm->program[vm->ip + 4];
      val  = _hvm_vm_register_read(vm, reg);
      assert(val->type == HVM_INTEGER);
      dest = (uint64_t)val->data.i64;
      reg  = vm->program[vm->ip + 5]; // Return register now
      vm->stack_depth += 1;
      frame = &vm->stack[vm->stack_depth];
      hvm_frame_initialize(frame);
      frame->return_addr     = vm->ip + 6; // Instruction 6 bytes long.
      frame->return_register = reg;
      hvm_vm_copy_regs(vm);
      vm->ip = dest;
      vm->top = frame;
      goto EXECUTE;
    case HVM_OP_INVOKEPRIMITIVE: // 1B OP | 1B REG | 1B REG
      AREG; BREG;
      key = _hvm_vm_register_read(vm, areg);// This is the symbol we need to look up.
      hvm_vm_copy_regs(vm);
      // fprintf(stderr, "CALLPRIMITIVE(%lld, $%d)\n", sym_id, breg);
      // FIXME: This may need to be smartened up.
      hvm_obj_ref *current_exc = vm->exception;// If there's a current exception
      val = hvm_vm_call_primitive(vm, key);
      // CHECK_EXCEPTION;
      if(vm->exception != current_exc) {
        goto EXCEPTION;
      }
      hvm_vm_register_write(vm, breg, val);
      IN_JIT(
        if(vm->top->trace != NULL) {
          hvm_jit_tracer_annotate_invokeprimitive_returned_type(vm, val);
        }
      )
      vm->ip += 2;
      break;

    case HVM_OP_CATCH: // 1B OP | 8B DEST | 1B REG
      dest = READ_U64(&vm->program[vm->ip + 1]);
      reg  = vm->program[vm->ip + 9];
      frame = &vm->stack[vm->stack_depth];
      frame->catch_addr     = dest;
      frame->catch_register = reg;
      vm->ip += 9;
      break;
    case HVM_OP_CLEARCATCH: // 1B OP
      frame = &vm->stack[vm->stack_depth];
      frame->catch_addr     = HVM_FRAME_EMPTY_CATCH;
      frame->catch_register = hvm_vm_reg_null();
      break;
    case HVM_OP_CLEAREXCEPTION: // 1B OP
      vm->exception = NULL;
      break;
    case HVM_OP_SETEXCEPTION: // 1B OP | 1B REG
      reg = vm->program[vm->ip + 1];
      // TODO: Throw new exception if no current exception.
      assert(vm->exception != NULL);
      // val = hvm_obj_for_exception(vm, vm->exception);
      val = vm->exception;
      hvm_vm_register_write(vm, reg, val);
      vm->ip += 1;
      break;
    case HVM_OP_THROW: // 1B OP | 1B REG
      AREG;
      // Get the object to be associated with the execption
      val = _hvm_vm_register_read(vm, areg);
      assert(val->type == HVM_STRUCTURE);
      // Create the exception and set the object
      // exc = hvm_new_exception();
      // exc->data = val;
      // Set the exception and jump to the handler
      vm->exception = exc;
      goto EXCEPTION;
    case HVM_OP_GETEXCEPTIONDATA: // 1B OP | 1B REG | 1B REG
      // AREG; BREG;
      // b = _hvm_vm_register_read(vm, breg);
      // assert(b->type == HVM_EXCEPTION);
      // exc = b->data.v;
      // val = exc->data;
      // hvm_vm_register_write(vm, areg, val);
      // vm->ip += 2;
      fprintf(stderr, "GETEXCEPTIONDATA is no longer supported.\n");
      assert(false);
      break;

    case HVM_OP_RETURN: // 1B OP | 1B REG
      reg = vm->program[vm->ip + 1];
      if(vm->stack_depth == 0) {
        msg = "Attempt to return from stack root";
        hvm_obj_ref *message = hvm_new_obj_ref_string_data(hvm_util_strclone(msg));
        // Raise the exception
        vm->exception = hvm_exception_new(vm, message);
        goto EXCEPTION;
      }
      // Current frame
      frame = vm->top;
      vm->ip = frame->return_addr;
      vm->stack_depth -= 1;
      vm->top = &vm->stack[vm->stack_depth];
      hvm_vm_register_write(vm, frame->return_register, _hvm_vm_register_read(vm, reg));
      // fprintf(stderr, "RETURN(0x%08llX) $%d -> $%d\n", frame->return_addr, reg, frame->return_register);
      goto EXECUTE;
    case HVM_OP_JUMP: // 1B OP | 4B DIFF
      diff = READ_I32(&vm->program[vm->ip + 1]);
      if(diff >= 0) {
        vm->ip += (uint64_t)diff;
      } else {
        // TODO: Check for reverse-overflow (ie. abs(diff) > vm->ip)
        vm->ip -= (uint64_t)(diff * -1);
      }
      goto EXECUTE;
    case HVM_OP_GOTO: // 1B OP | 8B DEST
      dest = READ_U64(&vm->program[vm->ip + 1]);
      vm->ip = dest;
      goto EXECUTE;
    case HVM_OP_GOTOADDRESS: // 1B OP | 1B REGDEST
      reg = vm->program[vm->ip + 1];
      val  = _hvm_vm_register_read(vm, reg);
      assert(val->type == HVM_INTEGER);
      i64  = val->data.i64;
      dest = (uint64_t)i64;
      vm->ip = dest;
      // fprintf(stderr, "GOTOADDRESS(0x%08llX)\n", dest);
      goto EXECUTE;
    case HVM_OP_IF: // 1B OP | 1B REG  | 8B DEST
      reg  = vm->program[vm->ip + 1];
      dest = READ_U64(&vm->program[vm->ip + 2]);
      val  = _hvm_vm_register_read(vm, reg);
      // Figure out whether or not we need to branch be seeing if the value
      // is falsey (null or integer zero).
      bool dont_branch = (val->type == HVM_NULL || (val->type == HVM_INTEGER && val->data.i64 == 0));
      IN_JIT(
        // Update the JIT branch predictor
        if(vm->top->trace != NULL) {
          // Update the trace to indicate whether or not this branch was taken.
          hvm_jit_tracer_annotate_if_branched(vm, !dont_branch);
        }
      )
      if(dont_branch) {
        // Falsey, add on the 9 bytes for the instruction parameters and continue onwards.
        vm->ip += 9;
        break;
      } else {
        // Truthy, go straight to destination.
        vm->ip = dest;
        goto EXECUTE;
      }

    case HVM_OP_LITINTEGER: // 1B OP | 1B REG | 8B LIT
      reg = vm->program[vm->ip + 1];
      i64 = READ_I64(&vm->program[vm->ip + 2]);
      val = hvm_new_obj_int();
      val->data.i64 = i64;
      hvm_obj_space_add_obj_ref(vm->obj_space, val);
      hvm_vm_register_write(vm, reg, val);
      vm->ip += 9;
      break;

    case HVM_OP_MOVE: // 1B OP | 1B REG | 1B REG
      AREG; BREG;
      hvm_vm_register_write(vm, areg, _hvm_vm_register_read(vm, breg));
      vm->ip += 2;
      break;

    case HVM_OP_SETSTRING:  // 1 = reg, 2-5 = const
    case HVM_OP_SETINTEGER: // 1B OP | 1B REG | 4B CONST
    case HVM_OP_SETFLOAT:
    case HVM_OP_SETSTRUCT:
    case HVM_OP_SETSYMBOL:
      // TODO: Type-checking or just do SETCONSTANT
      reg         = vm->program[vm->ip + 1];
      const_index = READ_U32(&vm->program[vm->ip + 2]);
      // fprintf(stderr, "0x%08llX  ", vm->ip);
      // fprintf(stderr, "SET $%u = const(%u)\n", reg, const_index);
      hvm_vm_register_write(vm, reg, hvm_vm_get_const(vm, const_index));
      vm->ip += 5;
      break;
    case HVM_OP_SETNULL: // 1B OP | 1B REG
      reg = vm->program[vm->ip + 1];
      hvm_vm_register_write(vm, reg, hvm_const_null);
      vm->ip += 1;
      break;

    // case HVM_OP_SETSYMBOL: // 1B OP | 1B REG | 4B CONST
    //   reg = vm->program[vm->ip + 1];
    //   const_index = READ_U32(&vm->program[vm->ip + 2]);
    //   vm->general_regs[reg] = hvm_vm_get_const(vm, const_index);

    case HVM_OP_SETLOCAL: // 1B OP | 1B REG   | 1B REG (local(A) = B)
      AREG; BREG;
      key = _hvm_vm_register_read(vm, areg);
      assert(key->type == HVM_SYMBOL);
      hvm_set_local(vm->top, key->data.u64, _hvm_vm_register_read(vm, breg));
      vm->ip += 2;
      break;
    case HVM_OP_GETLOCAL: // 1B OP | 1B REG   | 1B REG (A = local(B))
      AREG; BREG;
      key = _hvm_vm_register_read(vm, breg);
      assert(key->type == HVM_SYMBOL);
      val = hvm_get_local(vm->top, key->data.u64);
      if(val == NULL) {
        // Local not found
        char buff[256];// TODO: Danger, Will Robinson, buffer overflow!
        buff[0] = '\0';
        strcat(buff, "Undefined local: ");
        strcat(buff, hvm_desymbolicate(vm->symbols, key->data.u64));
        hvm_obj_ref *message = hvm_new_obj_ref_string_data(hvm_util_strclone(buff));
        vm->exception = hvm_exception_new(vm, message);
        goto EXCEPTION;
        // val = hvm_const_null;
      }
      hvm_vm_register_write(vm, areg, val);
      vm->ip += 2;
      break;

    case HVM_OP_SETGLOBAL: // 1B OP | 1B REG   | 1B REG
      AREG; BREG;
      key = _hvm_vm_register_read(vm, areg);
      assert(key->type == HVM_SYMBOL);
      hvm_set_global(vm, key->data.u64, _hvm_vm_register_read(vm, breg));
      vm->ip += 2;
      break;
    case HVM_OP_GETGLOBAL: // 1B OP | 1B REG   | 1B SYM
      AREG; BREG;
      key = _hvm_vm_register_read(vm, breg);
      assert(key->type == HVM_SYMBOL);
      hvm_vm_register_write(vm, areg, hvm_get_global(vm, key->data.u64));
      vm->ip += 2;
      break;

    case HVM_OP_GETCLOSURE: // 1B OP | 1B REG
      reg = vm->program[vm->ip + 1];
      // hvm_obj_ref* ref = hvm_new_obj_ref();
      // ref->type = HVM_STRUCTURE;
      // ref->data.v = vm->top->locals;
      hvm_obj_ref *ref = hvm_vm_build_closure(vm);
      hvm_obj_space_add_obj_ref(vm->obj_space, ref);
      hvm_vm_register_write(vm, reg, ref);
      vm->ip += 1;
      break;

    // MATH -----------------------------------------------------------------
    case HVM_OP_ADD:
    case HVM_OP_SUB:
    case HVM_OP_MUL:
    case HVM_OP_DIV:
    case HVM_OP_MOD: // 1B OP | 3B REGs
      // A = B + C
      AREG; BREG; CREG;
      a = NULL;
      b = _hvm_vm_register_read(vm, breg);
      c = _hvm_vm_register_read(vm, creg);
      // TODO: Add float support
      if(instr == HVM_OP_ADD)      { a = hvm_obj_int_add(b, c); }
      else if(instr == HVM_OP_SUB) { a = hvm_obj_int_sub(b, c); }
      else if(instr == HVM_OP_MUL) { a = hvm_obj_int_mul(b, c); }
      else if(instr == HVM_OP_DIV) { a = hvm_obj_int_div(b, c); }
      else if(instr == HVM_OP_MOD) { a = hvm_obj_int_mod(b, c); }
      if(a == NULL) {
        // Bad type
        vm->exception = hvm_new_operand_not_integer_exception(vm);
        goto EXCEPTION;
      }
      // Ensure the resulting integer is tracked in the GC
      hvm_obj_space_add_obj_ref(vm->obj_space, a);
      hvm_vm_register_write(vm, areg, a);
      vm->ip += 3;
      break;

    // MATHEMATICAL COMPARISON ----------------------------------------------
    case HVM_OP_LT:
    case HVM_OP_GT:
    case HVM_OP_LTE:
    case HVM_OP_GTE:
    case HVM_OP_EQ:  // 1B OP | 3B REGs
      // A = B < C
      AREG; BREG; CREG;
      a = NULL;
      b = _hvm_vm_register_read(vm, breg);
      c = _hvm_vm_register_read(vm, creg);
      if(instr == HVM_OP_LT)       { a = hvm_obj_int_lt(b, c); }
      else if(instr == HVM_OP_GT)  { a = hvm_obj_int_gt(b, c); }
      else if(instr == HVM_OP_LTE) { a = hvm_obj_int_lte(b, c); }
      else if(instr == HVM_OP_GTE) { a = hvm_obj_int_gte(b, c); }
      else if(instr == HVM_OP_EQ)  { a = hvm_obj_int_eq(b, c); }
      // TODO: Check if those comparison functions set an exception?
      if(a == NULL) {
        vm->exception = hvm_new_operand_not_integer_exception(vm);
        goto EXCEPTION;
      }
      hvm_obj_space_add_obj_ref(vm->obj_space, a);
      hvm_vm_register_write(vm, areg, a);
      vm->ip += 3;
      break;

    // BOOLEAN COMPARISON
    case HVM_OP_AND: // 1B OP | 3B REGS
      AREG; BREG; CREG;
      b = _hvm_vm_register_read(vm, breg);
      c = _hvm_vm_register_read(vm, creg);
      // Integer value for the result
      val = hvm_new_obj_int();
      // Do our truthy test
      val->data.i64 = (int64_t)((hvm_obj_is_truthy(b) && hvm_obj_is_truthy(c)) ? 1 : 0);
      // Add integer to GC object space and write to register
      hvm_obj_space_add_obj_ref(vm->obj_space, val);
      hvm_vm_register_write(vm, areg, a);
      vm->ip += 3;
      break;

    // ARRAYS ---------------------------------------------------------------
    case HVM_OP_ARRAYPUSH: // 1B OP | 2B REGS
      // A.push(B)
      AREG; BREG;
      a = _hvm_vm_register_read(vm, areg);
      b = _hvm_vm_register_read(vm, breg);
      hvm_obj_array_push(a, b);
      vm->ip += 2;
      break;
    case HVM_OP_ARRAYUNSHIFT: // 1B OP | 2B REGS
      // A.unshift(B)
      AREG; BREG;
      a = _hvm_vm_register_read(vm, areg);
      b = _hvm_vm_register_read(vm, breg);
      hvm_obj_array_unshift(a, b);
      vm->ip += 2;
      break;
    case HVM_OP_ARRAYSHIFT: // 1B OP | 2B REGS
      // A = B.shift()
      AREG; BREG;
      b = _hvm_vm_register_read(vm, breg);
      hvm_vm_register_write(vm, areg, hvm_obj_array_shift(b));
      vm->ip += 2;
      break;
    case HVM_OP_ARRAYPOP: // 1B OP | 2B REGS
      // A = B.pop()
      AREG; BREG;
      b = _hvm_vm_register_read(vm, breg);
      hvm_vm_register_write(vm, areg, hvm_obj_array_pop(b));
      vm->ip += 2;
      break;
    case HVM_OP_ARRAYGET: // 1B OP | 3B REGS
      // arrayget V A I -> V = A[I]
      AREG; BREG; CREG;
      arr = _hvm_vm_register_read(vm, breg);
      idx = _hvm_vm_register_read(vm, creg);
      hvm_vm_register_write(vm, areg, hvm_obj_array_get(arr, idx));
      vm->ip += 3;
      break;
    case HVM_OP_ARRAYSET: // 1B OP | 3B REGS
      // arrayset A I V -> A[I] = V
      AREG; BREG; CREG;
      arr = _hvm_vm_register_read(vm, areg);
      idx = _hvm_vm_register_read(vm, breg);
      val = _hvm_vm_register_read(vm, creg);
      hvm_obj_array_set(arr, idx, val);
      vm->ip += 3;
      break;
    case HVM_OP_ARRAYREMOVE: // 1B OP | 3B REGS
      // arrayremove V A I
      AREG; BREG; CREG;
      arr = _hvm_vm_register_read(vm, breg);
      idx = _hvm_vm_register_read(vm, creg);
      hvm_vm_register_write(vm, areg, hvm_obj_array_remove(arr, idx));
      vm->ip += 3;
      break;
    case HVM_OP_ARRAYNEW: // 1B OP | 2B REGS
      // arraynew A L
      AREG; BREG;
      val = _hvm_vm_register_read(vm, breg);
      hvm_obj_array *arr = hvm_new_obj_array_with_length(val);
      a = hvm_new_obj_ref();
      a->type = HVM_ARRAY;
      a->data.v = arr;
      hvm_obj_space_add_obj_ref(vm->obj_space, a);
      hvm_vm_register_write(vm, areg, a);
      vm->ip += 2;
      break;
    case HVM_OP_ARRAYLEN: // 1B OP | 2B REGS
      AREG; BREG;
      a = _hvm_vm_register_read(vm, breg);
      assert(a->type == HVM_ARRAY);
      val = hvm_obj_array_len(a);
      hvm_obj_space_add_obj_ref(vm->obj_space, val);
      hvm_vm_register_write(vm, areg, val);
      vm->ip += 2;
      break;


    // STRUCTS --------------------------------------------------------------
    case HVM_OP_STRUCTSET:
      // structset S K V
      AREG; BREG; CREG;
      strct = _hvm_vm_register_read(vm, areg);
      key   = _hvm_vm_register_read(vm, breg);
      val   = _hvm_vm_register_read(vm, creg);
      hvm_obj_struct_set(strct, key, val);
      // fprintf(stderr, "0x%08llX  ", vm->ip);
      // fprintf(stderr, "STRUCTSET $%u = $%u[$%u(%llu)]\n", areg, breg, creg, key->data.u64);
      // hvm_obj_print_structure(vm, strct->data.v);
      vm->ip += 3;
      break;
    case HVM_OP_STRUCTGET:
      // structget V S K
      AREG; BREG; CREG;
      // fprintf(stderr, "0x%08llX  ", vm->ip);
      // fprintf(stderr, "STRUCTGET $%u = $%u[$%u(%llu)]\n", areg, breg, creg, key->data.u64);
      strct = _hvm_vm_register_read(vm, breg);
      key   = _hvm_vm_register_read(vm, creg);
      if(strct->type != HVM_STRUCTURE) {
        // Bad type
        msg = "Attempting to get member of non-structure";
        hvm_obj_ref *message = hvm_new_obj_ref_string_data(hvm_util_strclone(msg));
        exc = hvm_exception_new(vm, message);

        hvm_location *loc = hvm_new_location();
        loc->name = hvm_util_strclone("hvm_structget");
        hvm_exception_push_location(vm, exc, loc);

        vm->exception = exc;
        goto EXCEPTION;
      }
      assert(strct->type == HVM_STRUCTURE);
      assert(key->type == HVM_SYMBOL);
      // hvm_obj_print_structure(vm, strct->data.v);
      val = hvm_obj_struct_get(strct, key);
      assert(val != NULL);
      hvm_vm_register_write(vm, areg, val);
      vm->ip += 3;
      break;
    case HVM_OP_STRUCTDELETE:
      // structdelete V S K
      AREG; BREG; CREG;
      strct = _hvm_vm_register_read(vm, breg);
      key   = _hvm_vm_register_read(vm, creg);
      hvm_vm_register_write(vm, areg, hvm_obj_struct_delete(strct, key));
      vm->ip += 3;
      break;
    case HVM_OP_STRUCTNEW:
      // structnew S
      AREG;
      hvm_obj_struct *s = hvm_new_obj_struct();
      strct = hvm_new_obj_ref();
      strct->type = HVM_STRUCTURE;
      strct->data.v = s;
      hvm_obj_space_add_obj_ref(vm->obj_space, strct);
      hvm_vm_register_write(vm, areg, strct);
      vm->ip += 1;
      break;
    case HVM_OP_STRUCTHAS:
      // structhas B S K
      fprintf(stderr, "STRUCTHAS not implemented yet!\n");
      goto end;

    case HVM_OP_SYMBOLICATE:
      // symbolicate SYM STR
      AREG; BREG;
      b = _hvm_vm_register_read(vm, breg);
      // Make sure we got a string
      if(b->type != HVM_STRING) {
        msg = "Symbolicate cannot handle non-string objects";
        hvm_obj_ref *message = hvm_new_obj_ref_string_data(hvm_util_strclone(msg));
        exc = hvm_exception_new(vm, message);
        vm->exception = exc;
        goto EXCEPTION;
      }
      hvm_obj_string *string = b->data.v;
      char *cstring = string->data;
      hvm_symbol_id sym = hvm_symbolicate(vm->symbols, cstring);
      val = hvm_new_obj_ref();
      val->type = HVM_SYMBOL;
      val->data.u64 = sym;
      hvm_obj_space_add_obj_ref(vm->obj_space, val);
      hvm_vm_register_write(vm, areg, val);
      vm->ip += 2;
      break;

    default:
      fprintf(stderr, "Unknown instruction: %u\n", instr);
  }
  vm->ip++;
  goto EXECUTE;

EXCEPTION:
  exc = vm->exception;
  assert(exc != NULL);
  assert(exc->type == HVM_STRUCTURE);
  hvm_exception_build_backtrace(exc, vm);
  // Climb stack looking for catch handler.
  depth = vm->stack_depth;
  while(1) {
    frame = &vm->stack[depth];
    if(frame->catch_addr != HVM_FRAME_EMPTY_CATCH) {
      // val = hvm_obj_for_exception(vm, exc);
      val = exc;
      hvm_vm_register_write(vm, frame->catch_register, val);
      // Resume execution at the exception handling address
      vm->ip = frame->catch_addr;
      // Clear the exception handler from the frame
      frame->catch_addr = HVM_FRAME_EMPTY_CATCH;
      frame->catch_register = hvm_vm_reg_null();
      goto EXECUTE;
    }
    if(depth == 0) { break; }
    depth--;
  }
  // No exception handler found
  hvm_exception_print(vm, exc);
  return;
