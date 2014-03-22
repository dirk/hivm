#ifndef HVM_SYMBOL_H
#define HVM_SYMBOL_H
/// @file symbol.h

// Start with 128 slots in the symbol table
// #define HVM_SYMBOL_TABLE_INITIAL_SIZE 2
#define HVM_SYMBOL_TABLE_INITIAL_SIZE 128
// Double in size when out of space
#define HVM_SYMBOL_TABLE_GROWTH_RATE  2

/// Internal symbol table.
typedef struct hvm_symbol_store {
  /// Heap data.
  struct hvm_symbol_store_entry** symbols;
  /// Next index in the heap.
  hvm_symbol_id next_id;
  /// Size of the allocated heap (in entries).
  uint64_t size;
} hvm_symbol_store;

/// Entry mapping ID to string in the symbol store.
typedef struct hvm_symbol_store_entry {
  /// Identifier (index) of the entry in the table.
  hvm_symbol_id id;
  /// String value/name of the symbol.
  char* value;
} hvm_symbol_store_entry;

/// Create a new symbol store
/// @memberof hvm_symbol_store
hvm_symbol_store *hvm_new_symbol_store();
/// Look up/add a string symbol to the symbol store
/// @memberof hvm_symbol_store
/// @returns  Symbol ID for the string
hvm_symbol_id hvm_symbolicate(hvm_symbol_store*, char*);

char *hvm_desymbolicate(hvm_symbol_store *st, hvm_symbol_id id);

#endif
