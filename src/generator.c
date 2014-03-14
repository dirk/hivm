#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include <glib.h>

#include "vm.h"
#include "generator.h"

hvm_gen *hvm_new_gen() {
  hvm_gen *gen = malloc(sizeof(hvm_gen));
  gen->items = g_array_new(TRUE, TRUE, sizeof(hvm_gen_item));
  return gen;
}
