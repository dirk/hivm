#ifndef HVM_DEBUG_C
#error debug-lua.include.c must only be included in debug.c
#endif
/// @file debug-lua.include.c
/// Included in debug.c to add all the Lua commands for debugging.

int hvm_lua_exit(lua_State *L) {
  fputs("(db) Exiting\n", stdout);
  hvm_debug_continue = false;
  return 0;// Pushed zero results onto the stack
}

/// Print the current debug backtrace
/// @memberof hvm_debugger
int hvm_lua_backtrace(lua_State *L) {
  hvm_vm *vm = lua_get_vm(L);
  // fprintf(stdout, "there! %p\n", vm);
  // Hackety hax backtrace building
  hvm_exception *exc = hvm_new_exception();
  hvm_exception_build_backtrace(exc, vm);
  // Get the backtrace out of the exception
  GArray *backtrace = exc->backtrace;
  // Then release the exception so we don't leak
  free(exc);

  hvm_print_backtrace(backtrace);
  // TODO: Free the backtrace
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
  hvm_vm *vm = lua_get_vm(L);
  hvm_debugger_set_breakpoint(vm, file, line);
  return 0;
}
