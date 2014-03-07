#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int main(int argc, char *argv[]) {
  // Testing a little memcpy
  unsigned char c[8];
  
  void *a = malloc(sizeof(void));
  memcpy(c, &a, 8);
  
  printf("a: %p\n", a);
  for(int i = 0; i < 8; i++) {
    printf("%d: %u\n", i, c[i]);
  }
  
  return 0;
}
