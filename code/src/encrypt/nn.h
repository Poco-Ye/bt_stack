#ifndef _NN_H_
#define _NN_H_

/* NN.H - header file for NN.C
 */

/* Copyright (C) RSA Laboratories, a division of RSA Data Security,
     Inc., created 1991. All rights reserved.
 */

/* Type definitions.
 */
typedef UINT4 MY_NN_DIGIT;
typedef UINT2 MY_NN_HALF_DIGIT;

/* Constants.

   Note: MAX_MY_NN_DIGITS is long enough to hold any RSA modulus, plus
   one more digit as required by R_GeneratePEMKeys (for n and phiN,
   whose lengths must be even). All natural numbers have at most
   MAX_MY_NN_DIGITS digits, except for double-length intermediate values
   in MY_NN_Mult (t), MY_NN_ModMult (t), MY_NN_ModInv (w), and MY_NN_Div (c).
 */
/* Length of digit in bits */
#define MY_NN_DIGIT_BITS 32
#define MY_NN_HALF_DIGIT_BITS 16
/* Length of digit in bytes */
#define MY_NN_DIGIT_LEN (MY_NN_DIGIT_BITS / 8)
/* Maximum length in digits */
#define MAX_MY_NN_DIGITS \
  ((MAX_RSA_MODULUS_LEN + MY_NN_DIGIT_LEN - 1) / MY_NN_DIGIT_LEN + 1)
/* Maximum digits */
#define MAX_MY_NN_DIGIT 0xffffffff
#define MAX_MY_NN_HALF_DIGIT 0xffff

/* Macros.
 */
#define LOW_HALF(x) ((x) & MAX_MY_NN_HALF_DIGIT)
#define HIGH_HALF(x) (((x) >> MY_NN_HALF_DIGIT_BITS) & MAX_MY_NN_HALF_DIGIT)
#define TO_HIGH_HALF(x) (((MY_NN_DIGIT)(x)) << MY_NN_HALF_DIGIT_BITS)
#define DIGIT_MSB(x) (unsigned int)(((x) >> (MY_NN_DIGIT_BITS - 1)) & 1)
#define DIGIT_2MSB(x) (unsigned int)(((x) >> (MY_NN_DIGIT_BITS - 2)) & 3)

/* CONVERSIONS
   MY_NN_Decode (a, digits, b, len)   Decodes character string b into a.
   MY_NN_Encode (a, len, b, digits)   Encodes a into character string b.

   ASSIGNMENTS
   MY_NN_Assign (a, b, digits)        Assigns a = b.
   MY_NN_ASSIGN_DIGIT (a, b, digits)  Assigns a = b, where b is a digit.
   MY_NN_AssignZero (a, b, digits)    Assigns a = 0.
   MY_NN_Assign2Exp (a, b, digits)    Assigns a = 2^b.

   ARITHMETIC OPERATIONS
   MY_NN_Add (a, b, c, digits)        Computes a = b + c.
   MY_NN_Sub (a, b, c, digits)        Computes a = b - c.
   MY_NN_Mult (a, b, c, digits)       Computes a = b * c.
   MY_NN_LShift (a, b, c, digits)     Computes a = b * 2^c.
   MY_NN_RShift (a, b, c, digits)     Computes a = b / 2^c.
   MY_NN_Div (a, b, c, cDigits, d, dDigits)  Computes a = c div d and b = c mod d.

   NUMBER THEORY
   MY_NN_Mod (a, b, bDigits, c, cDigits)  Computes a = b mod c.
   MY_NN_ModMult (a, b, c, d, digits) Computes a = b * c mod d.
   MY_NN_ModExp (a, b, c, cDigits, d, dDigits)  Computes a = b^c mod d.
   MY_NN_ModInv (a, b, c, digits)     Computes a = 1/b mod c.
   MY_NN_Gcd (a, b, c, digits)        Computes a = gcd (b, c).

   OTHER OPERATIONS
   MY_NN_EVEN (a, digits)             Returns 1 iff a is even.
   MY_NN_Cmp (a, b, digits)           Returns sign of a - b.
   MY_NN_EQUAL (a, digits)            Returns 1 iff a = b.
   MY_NN_Zero (a, digits)             Returns 1 iff a = 0.
   MY_NN_Digits (a, digits)           Returns significant length of a in digits.
   MY_NN_Bits (a, digits)             Returns significant length of a in bits.
 */
void MY_NN_Decode PROTO_LIST
  ((MY_NN_DIGIT *, unsigned int, unsigned char *, unsigned int));
void MY_NN_Encode PROTO_LIST
  ((unsigned char *, unsigned int, MY_NN_DIGIT *, unsigned int));

void MY_NN_Assign PROTO_LIST ((MY_NN_DIGIT *, MY_NN_DIGIT *, unsigned int));
void MY_NN_AssignZero PROTO_LIST ((MY_NN_DIGIT *, unsigned int));
void MY_NN_Assign2Exp PROTO_LIST ((MY_NN_DIGIT *, unsigned int, unsigned int));

MY_NN_DIGIT MY_NN_Add PROTO_LIST
  ((MY_NN_DIGIT *, MY_NN_DIGIT *, MY_NN_DIGIT *, unsigned int));
MY_NN_DIGIT MY_NN_Sub PROTO_LIST
  ((MY_NN_DIGIT *, MY_NN_DIGIT *, MY_NN_DIGIT *, unsigned int));
void MY_NN_Mult PROTO_LIST ((MY_NN_DIGIT *, MY_NN_DIGIT *, MY_NN_DIGIT *, unsigned int));
void MY_NN_Div PROTO_LIST
  ((MY_NN_DIGIT *, MY_NN_DIGIT *, MY_NN_DIGIT *, unsigned int, MY_NN_DIGIT *,
    unsigned int));
MY_NN_DIGIT MY_NN_LShift PROTO_LIST
  ((MY_NN_DIGIT *, MY_NN_DIGIT *, unsigned int, unsigned int));
MY_NN_DIGIT MY_NN_RShift PROTO_LIST
  ((MY_NN_DIGIT *, MY_NN_DIGIT *, unsigned int, unsigned int));

void MY_NN_Mod PROTO_LIST
  ((MY_NN_DIGIT *, MY_NN_DIGIT *, unsigned int, MY_NN_DIGIT *, unsigned int));
void MY_NN_ModMult PROTO_LIST
  ((MY_NN_DIGIT *, MY_NN_DIGIT *, MY_NN_DIGIT *, MY_NN_DIGIT *, unsigned int));
void MY_NN_ModExp PROTO_LIST
  ((MY_NN_DIGIT *, MY_NN_DIGIT *, MY_NN_DIGIT *, unsigned int, MY_NN_DIGIT *,
    unsigned int));
void MY_NN_ModInv PROTO_LIST
  ((MY_NN_DIGIT *, MY_NN_DIGIT *, MY_NN_DIGIT *, unsigned int));
void MY_NN_Gcd PROTO_LIST ((MY_NN_DIGIT *, MY_NN_DIGIT *, MY_NN_DIGIT *, unsigned int));

int MY_NN_Cmp PROTO_LIST ((MY_NN_DIGIT *, MY_NN_DIGIT *, unsigned int));
int MY_NN_Zero PROTO_LIST ((MY_NN_DIGIT *, unsigned int));
unsigned int MY_NN_Bits PROTO_LIST ((MY_NN_DIGIT *, unsigned int));
unsigned int MY_NN_Digits PROTO_LIST ((MY_NN_DIGIT *, unsigned int));
// add by liuxl for debug 20070612
MY_NN_DIGIT subdigitmult(MY_NN_DIGIT *a, MY_NN_DIGIT *b, MY_NN_DIGIT c, MY_NN_DIGIT *d, MY_NN_DIGIT digits);

#define MY_NN_ASSIGN_DIGIT(a, b, digits) {MY_NN_AssignZero (a, digits); a[0] = b;}
#define MY_NN_EQUAL(a, b, digits) (! MY_NN_Cmp (a, b, digits))
#define MY_NN_EVEN(a, digits) (((digits) == 0) || ! (a[0] & 1))

#endif

