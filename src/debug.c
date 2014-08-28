#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <glib.h>

#include "vm.h"
#include "object.h"
#include "symbol.h"
#include "debug.h"
#include "frame.h"
#include "exception.h"

#ifndef LUA_OK
#define LUA_OK 0
#endif

#define true  1
#define false 0

// Instance of Lua used during debugging
static lua_State *hvm_lua_state;

// Flag used by `hvm_debug_begin` to check if it should continue the REPL.
static bool hvm_debug_continue;

// Debugger functions for Lua
int hvm_lua_exit(lua_State*);
int hvm_lua_bt(lua_State*);

hvm_obj_ref *hvm_prim_debug_begin(hvm_vm *vm) {
  hvm_debug_begin();
  return hvm_const_null;
}

#define ADD_FUNCTION(FUNC, NAME) lua_pushcfunction(L, FUNC); \
                                 lua_setglobal(L, NAME);

void hvm_debug_setup(hvm_vm *vm) {
  // Setup the Lua instance
  hvm_lua_state = lua_open();
  lua_State *L = hvm_lua_state;
  luaopen_base(L);

  // Functions for Lua
  ADD_FUNCTION(hvm_lua_exit, "exit");
  ADD_FUNCTION(hvm_lua_bt, "bt");

  // Add a reference to our VM instance to the Lua C registry
  lua_pushstring(L, "hvm_vm");// key
  lua_pushlightuserdata(hvm_lua_state, vm);// value
  lua_settable(L, LUA_REGISTRYINDEX);

  // Add the primitives
  hvm_symbol_id symbol;
  symbol = hvm_symbolicate(vm->symbols, "debug_begin");
  hvm_obj_struct_internal_set(vm->primitives, symbol, (void*)hvm_prim_debug_begin);
}

void hvm_debug_prompt() {
  fputs("(db) ", stdout);
}

hvm_vm *lua_get_vm(lua_State *L) {
  lua_pushstring(L, "hvm_vm");// key
  lua_gettable(L, LUA_REGISTRYINDEX);
  void *vm = lua_touserdata(L, -1);
  lua_pop(L, 1);
  return (hvm_vm*)vm;
}

int hvm_lua_exit(lua_State *L) {
  fputs("(db) Exiting\n", stdout);
  hvm_debug_continue = false;
  return 0;// Pushed zero results onto the stack
}
// Printing backtrace
int hvm_lua_bt(lua_State *L) {
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

// Launch the Lua interpreter for debugging.
void hvm_debug_begin() {
  // TODO: Use readline or something more friendly
  static int BUFFER_SIZE = 256;
  char buffer[BUFFER_SIZE];

  int error = LUA_OK;
  hvm_debug_continue = true;

  while(hvm_debug_continue) {
    hvm_debug_prompt();
    if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) {
      break;
    }
    // Load our buffer as a function onto the top of the stack
    error = luaL_loadbuffer(hvm_lua_state, buffer, strlen(buffer), "line");
    // If it's okay then call the function at the top of the stack
    if (error == LUA_OK) {
      error = lua_pcall(hvm_lua_state, 0, 0, 0);
    }
    if(error != LUA_OK) {
      // Print and pop the error
      fprintf(stderr, "Error: %s\n", lua_tostring(hvm_lua_state, -1));
      lua_pop(hvm_lua_state, 1);
    }
  }
  lua_close(hvm_lua_state);
}

bool hvm_debug_before_instruction(hvm_vm *vm, byte instr) {
  return 1;
}
