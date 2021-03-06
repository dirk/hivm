#ifndef HVM_CHUNK_H
#define HVM_CHUNK_H

/// Locations where relative addresses have to be updated to absolute ones.
typedef struct hvm_chunk_relocation {
  /// Index into the data for the start of the 8-byte code address to be
  /// relocated (had the start of the chunk in the VM added to it).
  uint64_t index;
} hvm_chunk_relocation;

/// Marks constants in the chunk to be added to the VM's constant table.
typedef struct hvm_chunk_constant {
  /// Index of the symbol ID (4 byte unsigned integer).
  uint64_t            index;
  /// Actual (simplified) object; must be expanded into full object on load
  /// by using hvm_chunk_get_constant_object().
  struct hvm_obj_ref* object;
} hvm_chunk_constant;

/// Marks subroutines in the chunk to be added to the VM's symbol table.
typedef struct hvm_chunk_symbol {
  /// Index of the start of the subroutine identified by the symbol.
  uint64_t index;
  /// Name of that subroutine.
  char     *name;
} hvm_chunk_symbol;

typedef struct hvm_chunk_debug_entry {
  uint64_t      start;
  uint64_t      end;
  uint64_t      line;
  char          *name;
  char          *file;
  unsigned char flags;
} hvm_chunk_debug_entry;

/// @brief Chunk of instruction code and data (constants, etc.).
typedef struct hvm_chunk {
  // This is mostly inspired by the ELF format. There are three main sections
  // to a chunk: relocations, substitutions, symbols, and data.

  // RELOCATIONS (relocs)
  // Specifies points in the code where relative addresses need to be adjusted
  // to absolute addresses.
  hvm_chunk_relocation **relocs;// NULL-terminated array of relocs

  // CONSTANT SUBSTITUTIONS (consts)
  // Constants:
  //   Defines constant values and positions in the code where they are used.
  //   When the chunk is loaded these constants are pulled into the VM's
  //   constant table and the constant indexes in the code (specified by the 
  //   usage positions) are updated to use the VM's constant indexes.
  hvm_chunk_constant **constants;// NULL-terminated array

  // SYMBOLS
  // Defines symbols (ie. subroutines) and their locations to be inserted into
  // the VM's symbol table.
  hvm_chunk_symbol **symbols;// NULL-terminated
  
  // DEBUG ENTRIES
  hvm_chunk_debug_entry **debug_entries;// NULL-terminated

  // DATA
  /// Raw instructions.
  byte *data;
  /// Number of bytes used by the data.
  uint64_t size;
  /// Total size of the data (includes unused space).
  uint64_t capacity;
} hvm_chunk;

hvm_chunk *hvm_new_chunk();
void hvm_chunk_expand_if_necessary(hvm_chunk *chunk);

/// Expands a simplified object representation in a chunk constant into a full
/// constant object reference for use in the VM.
/// @memberof hvm_chunk_constant
hvm_obj_ref *hvm_chunk_get_constant_object(hvm_vm *vm, hvm_chunk_constant *cnst);

void hvm_chunk_disassemble(hvm_chunk *chunk);
void hvm_print_data(byte *data, uint64_t size);

#endif
