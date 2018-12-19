#ifndef MIFARE_ENCRYPT_H
#define MIFARE_ENCRYPT_H
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LF_POLY_ODD (0x29CE5C)
#define LF_POLY_EVEN (0x870804)
#define BIT(x, n) ((x) >> (n) & 1)



void NumToBytes(unsigned long long int n, unsigned int len, unsigned char* dest);
unsigned long long int BytesToNum(unsigned char* src, unsigned int len);
unsigned short UpdateCrc(unsigned char ch, unsigned short *lpwCrc);
unsigned char ComputeCrc(char *Data, int Length, unsigned char *TransmitFirst, unsigned char *TransmitSecond);
unsigned int SwapEndian32(unsigned char *pt);
void AddOddParity(unsigned char *data, int InByteLength, int* OutByteLength, int* OutBitLength);
void AddParity(unsigned char *data, int InByteLength, unsigned char* Parity, int* OutByteLength, int* OutBitLength);
void RemoveParity(unsigned char *data, int InByteLength, int InBitStart, int* OutByteLength);	
unsigned char ODD_PARITY(const unsigned char bt);

struct Crypto1State {unsigned int odd, even;};
struct Crypto1State * crypto1_create(unsigned long long int key);
unsigned char crypto1_bit(struct Crypto1State *s, unsigned char in, int is_encrypted);
unsigned char crypto1_byte(struct Crypto1State *s, unsigned char in, int is_encrypted);
unsigned int crypto1_word(struct Crypto1State *s, unsigned int in, int is_encrypted);
unsigned int prng_successor(unsigned int x, unsigned int n);

int parity(unsigned int x);
int filter(unsigned int const x);

#ifdef __cplusplus
}
#endif

#endif
