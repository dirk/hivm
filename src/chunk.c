#include <stdlib.h>

#include "vm.h"
#include "chunk.h"

hvm_chunk *hvm_new_chunk() {
  hvm_chunk *chunk = malloc(sizeof(hvm_chunk));
  chunk->data = NULL;
  chunk->size = 0;
  chunk->capacity = 0;
  return chunk;
}

void hvm_chunk_expand(hvm_chunk *chunk) {
  chunk->capacity += 1024;// TODO: Refactor this.
  chunk->data = realloc(chunk->data, sizeof(byte) * chunk->capacity);
}

void hvm_chunk_expand_if_necessary(hvm_chunk *chunk) {
  if((chunk->size + 16) >= chunk->capacity) {
    hvm_chunk_expand(chunk);
  }
}
