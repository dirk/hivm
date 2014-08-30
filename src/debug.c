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
#include "chunk.h"
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
int hvm_lua_backtrace(lua_State*);
int hvm_lua_breakpoint(lua_State*);

hvm_obj_ref *hvm_prim_debug_begin(hvm_vm *vm) {
  hvm_debug_begin();
  return hvm_const_null;
}

#define ADD_FUNCTION(FUNC, NAME) lua_pushcfunction(L, FUNC); \
                                 lua_setglobal(L, NAME);

void hvm_debug_setup_lua(hvm_vm *vm) {
  // Setup the Lua instance
  hvm_lua_state = lua_open();
  lua_State *L = hvm_lua_state;
  luaopen_base(L);

  // Functions for Lua
  ADD_FUNCTION(hvm_lua_exit, "exit");
  ADD_FUNCTION(hvm_lua_backtrace, "backtrace");
  ADD_FUNCTION(hvm_lua_breakpoint, "breakpoint");

  // Add a reference to our VM instance to the Lua C registry
  lua_pushstring(L, "hvm_vm");// key
  lua_pushlightuserdata(hvm_lua_state, vm);// value
  lua_settable(L, LUA_REGISTRYINDEX);
}

void hvm_debug_setup(hvm_vm *vm) {
  // Setup our debugger instance
  hvm_debugger *debugger = malloc(sizeof(hvm_debugger));
  vm->debugger = debugger;
  // We'll store the whole struct in the GArray (instead of just a pointer)
  // for spatial locality.
  debugger->breakpoints = g_array_new(TRUE, TRUE, sizeof(hvm_debug_breakpoint));

  hvm_debug_setup_lua(vm);

  // Add the primitives to the VM
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

#define HVM_DEBUG_C
#include "debug-lua.include.c"

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
  // lua_close(hvm_lua_state);
}

bool hvm_debug_before_instruction(hvm_vm *vm) {
  // Get the debugger and the instruction pointer out of the VM
  hvm_debugger *debugger = (hvm_debugger*)vm->debugger;
  uint64_t ip = vm->ip;
  hvm_debug_breakpoint *bp;
  GArray *breakpoints = debugger->breakpoints;
  for(unsigned int i = 0; i < breakpoints->len; i++) {
    bp = &g_array_index(breakpoints, hvm_debug_breakpoint, i);
    if(bp->start <= ip && ip <= bp->end) {
      debugger->current_breakpoint = bp;
      hvm_debug_begin();
      return 1;
    }
  }
  return 1;
}

hvm_chunk_debug_entry *hvm_debug_find_debug_entry_for_breakpoint(hvm_vm *vm, char *file, uint64_t line) {
  hvm_chunk_debug_entry *de;
  for(uint64_t i = 0; i < vm->debug_entries_size; i++) {
    de = &vm->debug_entries[i];
    if(strcmp(de->file, file) == 0 && de->line == line) {
      return de;
    }
  }
  return NULL;
}

void hvm_debugger_set_breakpoint(hvm_vm *vm, char *file, uint64_t line) {
  hvm_debugger *debugger = vm->debugger;
  hvm_chunk_debug_entry *entry = hvm_debug_find_debug_entry_for_breakpoint(vm, file, line);
  if(entry == NULL) {
    fprintf(stderr, "Unable to set breakpoint for file '%s' at line %llu\n", file, line);
    return;
  }

  hvm_debug_breakpoint bp = {
    .file  = file,
    .line  = line,
    .start = entry->start,
    .end   = entry->end
  };
  g_array_append_val(debugger->breakpoints, bp);
}
