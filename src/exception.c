#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include <glib.h>

#include "vm.h"
#include "symbol.h"
#include "object.h"
#include "frame.h"
#include "exception.h"

hvm_exception *hvm_new_exception() {
  hvm_exception *exc = malloc(sizeof(hvm_exception));
  exc->message = NULL;
  exc->backtrace = g_array_new(TRUE, TRUE, sizeof(hvm_frame*));
  return exc;
}
