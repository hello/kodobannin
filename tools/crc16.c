#include "crc16.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv)
{
  if(argc < 2) {
    printf("usage: %s <filename>\n", argv[0]);
    return 1;
  }

  char* path = argv[1];

  FILE* file = fopen(path, "rb");
  fseek(file, 0, SEEK_END);
  uint32_t file_size = ftell(file);
  fseek(file, 0, SEEK_SET);

  uint8_t* data = malloc(file_size);
  fread(data, file_size, 1, file);
  fclose(file);

  uint16_t crc = crc16_compute(data, file_size, NULL);
  printf("%x\n", crc);

  free(data);
}
