#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "vm.h"
#include "debug.h"

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

void hvm_debug_setup() {
  hvm_lua_state = lua_open();
  luaopen_base(hvm_lua_state);
  // Add our exit() function
  lua_pushcfunction(hvm_lua_state, hvm_lua_exit);
  lua_setglobal(hvm_lua_state, "exit");
}

void hvm_debug_prompt() {
  fputs("(db) ", stdout);
}
int hvm_lua_exit(lua_State *L) {
  fputs("(db) Exiting", stdout);
  hvm_debug_continue = false;
  return 0;// Pushed zero results onto the stack
}

// Launch the Lua interpreter for debugging.
void hvm_debug_begin() {
  // TODO: Use readline or something more friendly
  static int BUFFER_SIZE = 256;
  char buffer[BUFFER_SIZE];

  int error = LUA_OK;
  hvm_debug_continue = true;

  hvm_debug_prompt();
  while(hvm_debug_continue && fgets(buffer, BUFFER_SIZE, stdin) != NULL) {
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
    hvm_debug_prompt();
  }
  lua_close(hvm_lua_state);
}

bool hvm_debug_before_instruction(hvm_vm *vm, byte instr) {
  hvm_debug_setup();
  hvm_debug_begin();
  return 0;
}
