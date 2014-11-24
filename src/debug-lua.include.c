#ifndef HVM_DEBUG_C
#error debug-lua.include.c must only be included in debug.c
#endif
/// @file debug-lua.include.c
/// Included in debug.c to add all the Lua commands for debugging.

hvm_vm *_hvm_lua_get_vm(lua_State *L) {
  lua_pushstring(L, "hvm_vm");// key
  lua_gettable(L, LUA_REGISTRYINDEX);
  void *vm = lua_touserdata(L, -1);
  lua_pop(L, 1);
  return (hvm_vm*)vm;
}

int hvm_lua_continue(lua_State *L) {
  //fputs("(db) Exiting\n", stdout);
  hvm_debug_continue = false;
  return 0;// Pushed zero results onto the stack
}

/// Print the current debug backtrace
/// @memberof hvm_debugger
int hvm_lua_backtrace(lua_State *L) {
  hvm_vm *vm = _hvm_lua_get_vm(L);
  // fprintf(stdout, "there! %p\n", vm);
  // Hackety hax backtrace building
  hvm_obj_ref *exc = hvm_exception_new(vm, NULL);
  hvm_exception_build_backtrace(exc, vm);
  // Get the backtrace out of the exception
  hvm_symbol_id sym = hvm_symbolicate(vm->symbols, "backtrace");
  hvm_obj_ref *backtrace = hvm_obj_struct_internal_get(exc->data.v, sym);
  if(backtrace != NULL) {
    hvm_print_backtrace_array(backtrace);
  } else {
    fprintf(stderr, "No backtrace found!\n");
  }
  // Then release the exception so we don't leak
  hvm_obj_free(exc);
  return 0;
}

/// Sets a breakpoint
/// @memberof hvm_debugger
int hvm_lua_breakpoint(lua_State *L) {
  // Argument 1 is the file
  const char *file_from_lua = luaL_checkstring(L, 1);
  unsigned long length = strlen(file_from_lua);
  char *file = malloc(sizeof(char) * (length + 1));
  strcpy(file, file_from_lua);
  // Argument 2 is the line number
  lua_Integer line_from_lua = luaL_checkinteger(L, 2);
  // Explicit conversion to 64-bit
  uint64_t line = (uint64_t)line_from_lua;
  // Set the breakpoint
  hvm_vm *vm = _hvm_lua_get_vm(L);
  hvm_debugger_set_breakpoint(vm, file, line);
  return 0;
}

/// Print the registers
/// @memberof hvm_debugger
int hvm_lua_registers(lua_State *L) {
  hvm_vm *vm = _hvm_lua_get_vm(L);
  for(unsigned int i = 0; i < HVM_GENERAL_REGISTERS; i++) {
    hvm_obj_ref *ref = vm->general_regs[i];
    if(ref != hvm_const_null) {
      const char *name = hvm_human_name_for_obj_type(ref->type);
      fprintf(stderr, "g%-3d = %p (%s)\n", i, ref, name);
    }
  }
  return 0;
}


