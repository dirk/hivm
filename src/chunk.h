#ifndef HVM_CHUNK_H
#define HVM_CHUNK_H

/// @brief Chunk of instruction code and data (constants, etc.).
typedef struct hvm_chunk {
  // This is mostly inspired by the ELF format. There are three main sections
  // to a chunk: relocations, substitutions, and data.
  
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
  
  // DATA
} hvm_chunk;

#endif
