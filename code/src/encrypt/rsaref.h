/* RSAREF.H - header file for RSAREF cryptographic toolkit
 */

/* Copyright (C) RSA Laboratories, a division of RSA Data Security,
     Inc., created 1991. All rights reserved.
 */

#ifndef _RSAREF_H_
#define _RSAREF_H_ 1

#include "md2.h"
#include "md5.h"
#include "Rsa.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Message-digest algorithms.
 */
#define DA_MD2 3
#define DA_MD5 5

/* Encryption algorithms to be ored with digest algorithm in Seal and Open.
 */
#define EA_DES_CBC 1
#define EA_DES_EDE2_CBC 2
#define EA_DES_EDE3_CBC 3
#define EA_DESX_CBC 4

/* Maximum lengths of encoded and encrypted content, as a function of
   content length len. Also, inverse functions.
 */
#define ENCODED_CONTENT_LEN(len) (4*(len)/3 + 3)
#define ENCRYPTED_CONTENT_LEN(len) ENCODED_CONTENT_LEN ((len)+8)
#define DECODED_CONTENT_LEN(len) (3*(len)/4 + 1)
#define DECRYPTED_CONTENT_LEN(len) (DECODED_CONTENT_LEN (len) - 1)

/* Maximum lengths of signatures, encrypted keys, encrypted
   signatures, and message digests.
 */
#define MAX_SIGNATURE_LEN MAX_RSA_MODULUS_LEN
#define MAX_PEM_SIGNATURE_LEN ENCODED_CONTENT_LEN (MAX_SIGNATURE_LEN)
#define MAX_ENCRYPTED_KEY_LEN MAX_RSA_MODULUS_LEN
#define MAX_PEM_ENCRYPTED_KEY_LEN ENCODED_CONTENT_LEN (MAX_ENCRYPTED_KEY_LEN)
#define MAX_PEM_ENCRYPTED_SIGNATURE_LEN \
  ENCRYPTED_CONTENT_LEN (MAX_SIGNATURE_LEN)
#define MAX_DIGEST_LEN 16

/* Maximum length of Diffie-Hellman parameters.
 */
#define DH_PRIME_LEN(bits) (((bits) + 7) / 8)

/* Error codes.
 */
#define RE_CONTENT_ENCODING 0x0400
#define RE_DATA 0x0401
#define RE_DIGEST_ALGORITHM 0x0402
#define RE_ENCODING 0x0403
#define RE_KEY 0x0404
#define RE_KEY_ENCODING 0x0405
#define RE_LEN 0x0406
#define RE_MODULUS_LEN 0x0407
#define RE_NEED_RANDOM 0x0408
#define RE_PRIVATE_KEY 0x0409
#define RE_PUBLIC_KEY 0x040a
#define RE_SIGNATURE 0x040b
#define RE_SIGNATURE_ENCODING 0x040c
#define RE_ENCRYPTION_ALGORITHM 0x040d

/* Random structures.
 */
int R_RandomInit PROTO_LIST ((R_RANDOM_STRUCT *));
int R_RandomUpdate PROTO_LIST
  ((R_RANDOM_STRUCT *, unsigned char *, unsigned int));
int R_GetRandomBytesNeeded PROTO_LIST ((unsigned int *, R_RANDOM_STRUCT *));
void R_RandomFinal PROTO_LIST ((R_RANDOM_STRUCT *));

/* Routines supplied by the implementor.
 */
void R_memset PROTO_LIST ((POINTER, int, unsigned int));
void R_memcpy PROTO_LIST ((POINTER, POINTER, unsigned int));
int R_memcmp PROTO_LIST ((POINTER, POINTER, unsigned int));

#ifdef __cplusplus
}
#endif

#endif


