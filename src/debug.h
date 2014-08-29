#ifndef HVM_DEBUG_H
#define HVM_DEBUG_H
/// @file debug.h

#ifndef bool
#define bool char
#endif

typedef struct hvm_debugger {
#ifdef GLIB_MAJOR_VERSION
  GArray *breakpoints;
#else
  /// @cond
  void *breakpoints;
  /// @endcond
#endif
  struct hvm_debug_breakpoint *current_breakpoint;
} hvm_debugger;

typedef struct hvm_debug_breakpoint {
  char *file;
  uint64_t line;
  uint64_t start;
  uint64_t end;
} hvm_debug_breakpoint;

/// Called by `hvm_new_vm` to add the debugger primitives to the VM.
void hvm_debug_setup(hvm_vm*);

/// Hook called by `hvm_vm_run` before it begins executing an instruction.
/// @param    vm
/// @param    instr  Instruction the VM is about to excute.
/// @retval   bool   Whether the VM should continue execution.
bool hvm_debug_before_instruction(hvm_vm*);

void hvm_debug_begin();

void hvm_debugger_set_breakpoint(hvm_vm *vm, char *file, uint64_t line);

#endif
