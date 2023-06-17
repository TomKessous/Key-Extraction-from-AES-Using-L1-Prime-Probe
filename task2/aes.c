#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include "aes.h"
#include "time.h"
#include "PrimeProbe.h"

#define AESSIZE 16
#define NUM_OF_WAYS 8
#define FW 1
typedef uint8_t aes_t[AESSIZE];


void tobinary(char *data, aes_t aes) {
  assert(strlen(data)==AESSIZE*2);
  unsigned int x;
  for (int i = 0; i < AESSIZE; i++) {
    sscanf(data+i*2, "%2x", &x);
    aes[i] = x;
  }
}

char *toString(aes_t aes) {
  char buf[AESSIZE * 2 + 1];
  for (int i = 0; i < AESSIZE; i++)
    sprintf(buf + i*2, "%02x", aes[i]);
  return strdup(buf);
}

void randaes(aes_t aes) {
  for (int i = 0; i < AESSIZE; i++)
    aes[i] = rand() & 0xff;
}



int main(int argc, char *argv[]) {
  srand(time(NULL));
  if (argc != 3) {
      printf("Usage: %s <AES key> <number of encryptions>\n", argv[0]);
      return 1;
  }

  FILE *aes_output = fopen("aes_output.txt", "w");
  if (aes_output == NULL) {
      printf("Failed to open the file.\n");
      return 1;
  }
  //Prepare Prime+Probe attack.
  struct Memory mem = ppinit();
  uint64_t cs[NUM_OF_SETS];
  int numEncryptions = atoi(argv[2]);
  aes_t key, input, output;
  char *input_str, *output_str;
  tobinary(argv[1], key);
  AES_KEY aeskey;
  private_AES_set_encrypt_key(key, 128, &aeskey);

  
  for(int i = 0;i < numEncryptions; i++){
    //Generate random input.
    randaes(input);
    //Prime L1
    prime(mem.evsets1, NUM_OF_WAYS,FW);
    //Encrypt!
    AES_encrypt(input, output, &aeskey);
    //Probe the L1
    probe(mem.evsets1, cs, NUM_OF_WAYS,FW);
    //Convert input and output to strings.
    input_str = toString(input);
    output_str = toString(output);
    fprintf(aes_output,"%s %s ", input_str, output_str);
    for(int cache_set=0; cache_set < NUM_OF_SETS; cache_set++) fprintf(aes_output,"%ld ", cs[cache_set]);
    fprintf(aes_output,"\n");
    free(input_str);
    free(output_str);

  }
  fclose(aes_output);
}
