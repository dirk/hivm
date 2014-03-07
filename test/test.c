#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// gcc test.c -g -o test

void byte_fiddling() {
  uint32_t i = 0x0800;
  printf("i: 0x%lx\n", (unsigned long)i);
  
  void *p = &i;
  for(int o = 0; o < 4; o++) {
    // These are equivalent.
    //unsigned char c = ((unsigned char*)(&i))[o];
    unsigned char c  = *((unsigned char*)&i + o);
    // Have to cast to unsigned char before offsetting. This will make it go by
    // bytes instead of ints (4 bytes).
    printf("c[%d]: 0x%hhx\n", o, c);
  }
}

void memcpy_fiddling() {
  // Testing a little memcpy
  unsigned char c[8];
  
  void *a = malloc(sizeof(void));
  memcpy(c, &a, 8);
  
  printf("a: %p\n", a);
  for(int i = 0; i < 8; i++) {
    printf("%d: %u\n", i, c[i]);
  }
  
  void *b;
  memcpy(&b, c, 8);
  printf("b: %p\n", b);
  
  uint64_t d;
  d = (uint64_t)b;
  printf("d: 0x%llx\n", d);
  printf("d: %lld\n", d);
  
  // Make compiler treat it as a double without coercion/conversion.
  double f = *((double*)&d);
  printf("f: %f\n", f);
  // Proof that no conversion occurred.
  uint64_t g = *((uint64_t*)&f);
  printf("g: %lld\n", g);
}

int main(int argc, char *argv[]) {
  // memcpy_fiddling();
  byte_fiddling();
  return 0;
}
