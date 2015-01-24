#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>

#include "hvm.h"
#include "hvm_symbol.h"
#include "hvm_object.h"
#include "hvm_chunk.h"
#include "hvm_generator.h"
#include "hvm_bootstrap.h"

// Each file is an independent set of assertions. The preamble provides the
// functionality required for each test in a suite.

// Assertion reports ("." for pass, "x" for failure) are written to STDOUT and
// failure messages are written to STDERR with a trailing new line separating
// them.

#ifndef MAX_ASSERTION_ERRS
#define MAX_ASSERTION_ERRS 100
#endif

int _assertion_errc = 0;
const char *_assertion_errv[MAX_ASSERTION_ERRS];

void assert_true(bool val, const char *msg) {
  int i = _assertion_errc;
  if(!val) {
    _assertion_errv[i] = msg;
    _assertion_errc += 1;
    fputs("x", stdout);
    fprintf(stderr, "%s\n", msg);
  } else {
    fputs(".", stdout);
  }
}

int done() {
  fputs("\n", stdout);
  return (_assertion_errc > 0) ? 1 : 0;
}


// Utilities ------------------------------------------------------------------

hvm_vm *run_chunk(hvm_chunk *chunk) {
  hvm_vm *vm = hvm_new_vm();
  hvm_bootstrap_primitives(vm);
  hvm_vm_load_chunk(vm, chunk);
  hvm_vm_run(vm);
  return vm;
}
hvm_vm *gen_chunk_and_run(hvm_gen *gen) {
  hvm_chunk *chunk = hvm_gen_chunk(gen);
  hvm_vm *vm = run_chunk(chunk);
  return vm;
}
