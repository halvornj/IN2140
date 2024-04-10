#include <stdio.h>

int main() {
  FILE *fptr;

  // Open a file in read mode
  fptr = fopen("test", "rb");
  unsigned char buffer[10];
  fread(buffer, sizeof(buffer), 1, fptr);

  for(int i = 0; i<10; i++)
    printf("%u ", buffer[i]); // prints a series of bytes


  return 0;
}



