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

// Instance of Lua used during debugging
static lua_State *L;

void hvm_debug_setup() {
  L = lua_open();
  luaopen_base(L);
}

void debug_prompt() {
  fputs("(db) ", stdout);
}

// Launch the Lua interpreter for debugging.
void hvm_debug_begin() {
  static int BUFFER_SIZE = 256;
  // TODO: Use readline or something more friendly
  char buffer[BUFFER_SIZE];
  int error;

  debug_prompt();
  while(fgets(buffer, BUFFER_SIZE, stdin) != NULL) {
    // Load our buffer as a function onto the top of the stack
    error = luaL_loadbuffer(L, buffer, strlen(buffer), "line");
    // If it's okay then call the function at the top of the stack
    if (error == LUA_OK) {
      error = lua_pcall(L, 0, 0, 0);
    }
    if(error != LUA_OK) {
      // Print and pop the error
      fprintf(stderr, "Error: %s\n", lua_tostring(L, -1));
      lua_pop(L, 1);
    }
    debug_prompt();
  }
  lua_close(L);
}

bool hvm_debug_before_instruction(hvm_vm *vm, byte instr) {
  hvm_debug_setup();
  hvm_debug_begin();
  return 0;
}
