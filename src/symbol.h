#ifndef HVM_SYMBOL_H
#define HVM_SYMBOL_H
/// @file symbol.h

// Start with 128 slots in the symbol table
// #define HVM_SYMBOL_TABLE_INITIAL_SIZE 2
#define HVM_SYMBOL_TABLE_INITIAL_SIZE 128
// Double in size when out of space
#define HVM_SYMBOL_TABLE_GROWTH_RATE  2

typedef struct hvm_symbol_table {
  struct hvm_symbol_table_entry** symbols;
  uint64_t                next_id;
  uint64_t                size;
} hvm_symbol_table;

typedef struct hvm_symbol_table_entry {
  uint64_t id;
  char*    value;
} hvm_symbol_table_entry;

hvm_symbol_table *new_hvm_symbol_table();
uint64_t hvm_symbolicate(hvm_symbol_table*, char*);

#endif
