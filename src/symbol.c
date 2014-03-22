#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "vm.h"
#include "symbol.h"

hvm_symbol_store *hvm_new_symbol_store() {
  hvm_symbol_store *st = malloc(sizeof(hvm_symbol_store));
  st->next_id = 1;
  st->size = HVM_SYMBOL_TABLE_INITIAL_SIZE;
  st->symbols = malloc(sizeof(hvm_symbol_store_entry*) * st->size);
  return st;
}

char *hvm_sym_strclone(char *str) {
  size_t len = strlen(str);
  char  *clone = malloc(sizeof(char) * (size_t)(len + 1));
  strcpy(clone, str);
  return clone;
}

void hvm_symbol_store_expand(hvm_symbol_store *st) {
  st->size = st->size * HVM_SYMBOL_TABLE_GROWTH_RATE;
  st->symbols = realloc(st->symbols, sizeof(hvm_symbol_store_entry*) * st->size);
}

hvm_symbol_store_entry *hvm_symbol_store_add(hvm_symbol_store *st, char *value) {
  hvm_symbol_id id = st->next_id;
  hvm_symbol_store_entry *entry = malloc(sizeof(hvm_symbol_store_entry));
  entry->value = hvm_sym_strclone(value);
  entry->id = id;
  if(id >= (st->size - 1)) {
    hvm_symbol_store_expand(st);
  }
  st->symbols[id] = entry;
  st->next_id += 1;
  // fprintf(stderr, "symbol_add: %llu = %s\n", entry->id, entry->value);
  return entry;
}

hvm_symbol_id hvm_symbolicate(hvm_symbol_store *st, char *value) {
  hvm_symbol_store_entry *entry;
  hvm_symbol_id i;
  for(i = 1; i < st->next_id; i++) {
    entry = st->symbols[i];
    if(strcmp(entry->value, value) == 0) {
      assert(i == entry->id);
      return entry->id;
    }
  }
  entry = hvm_symbol_store_add(st, value);
  return entry->id;
}

char *hvm_desymbolicate(hvm_symbol_store *st, hvm_symbol_id id) {
  hvm_symbol_store_entry *entry;
  hvm_symbol_id i;
  for(i = 1; i < st->next_id; i++) {
    entry = st->symbols[i];
    if(id == entry->id) {
      return entry->value;
    }
  }
  return NULL;
}
