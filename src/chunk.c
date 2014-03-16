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
