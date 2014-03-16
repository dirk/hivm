#ifndef HVM_CHUNK_H
#define HVM_CHUNK_H

typedef struct hvm_chunk_relocation {
  /// Index into the data for the start of the 8-byte code address to be
  /// relocated (had the start of the chunk in the VM added to it).
  uint64_t index;
} hvm_chunk_relocation;

typedef struct hvm_chunk_constant {
  uint64_t            index;
  struct hvm_obj_ref* object;
} hvm_chunk_constant;

/// @brief Chunk of instruction code and data (constants, etc.).
typedef struct hvm_chunk {
  // This is mostly inspired by the ELF format. There are three main sections
  // to a chunk: relocations, substitutions, symbols, and data.
  
  // RELOCATIONS (relocs)
  // Specifies points in the code where relative addresses need to be adjusted
  // to absolute addresses.
  
  // SUBSTITUTIONS (subs)
  // Constants:
  //   Defines constant values and positions in the code where they are used.
  //   When the chunk is loaded these constants are pulled into the VM's
  //   constant table and the constant indexes in the code (specified by the 
  //   usage positions) are updated to use the VM's constant indexes.
  // Symbols:
  //   1. Resolves symbols used in SETSYMBOL instructions to their proper IDs.
  //   2. Resolves symbols used in CALLs to the proper addresses through the
  //      VM's subroutine symbol table (this of course requires those symbols
  //      and addresses to already be known, making this really only useful
  //      during various bootstrappings when load-order is well-known and
  //      reliable).
  
  // SYMBOLS
  // Defines symbols (ie. subroutines) and their locations to be inserted into
  // the VM's symbol table.
  
  // DATA
  /// Raw instructions.
  byte *data;
  /// Number of bytes used by the data.
  uint64_t size;
  /// Total size of the data (includes unused space).
  uint64_t capacity;
} hvm_chunk;

hvm_chunk *hvm_new_chunk();

#endif
