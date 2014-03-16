#include <stdlib.h>

#include "chunk.h"

hvm_chunk *hvm_new_chunk() {
  hvm_chunk *chunk = malloc(sizeof(hvm_chunk));
  return chunk;
}
