/* NN.C - natural numbers routines
 */

/* Copyright (C) RSA Laboratories, a division of RSA Data Security,
     Inc., created 1991. All rights reserved.
 */

#include "global.h"
#include "rsaref.h"
#include "nn.h"
#include "digit.h"

static MY_NN_DIGIT MY_NN_AddDigitMult PROTO_LIST
  ((MY_NN_DIGIT *, MY_NN_DIGIT *, MY_NN_DIGIT, MY_NN_DIGIT *, unsigned int));
static MY_NN_DIGIT MY_NN_SubDigitMult PROTO_LIST
  ((MY_NN_DIGIT *, MY_NN_DIGIT *, MY_NN_DIGIT, MY_NN_DIGIT *, unsigned int));

static unsigned int MY_NN_DigitBits PROTO_LIST ((MY_NN_DIGIT));

/****************************************************************************
  函数名     :  void dmult(uint a, uint b, uint *cHigh, uint *cLow)
  描述       :  32位整数乘法运算，该函数运行于windows下
  输入参数   :  1、uint a : 乘数
                2、uint b : 乘数
  输出参数   :  1、uint *cHigh : 乘积高32位
                2、uint *cLow : 乘积低32位
  返回值     :  无
  修改历史   :
      修改人     修改时间    修改版本号   修改原因
  1、 黄俊斌     2007-01-05  v1.0         创建
****************************************************************************/
void dmult(MY_NN_DIGIT a, MY_NN_DIGIT b, MY_NN_DIGIT *cHigh, MY_NN_DIGIT *cLow)
{
	__asm( "STMFD   sp!, {r4-r5}" );
    __asm( "UMULL   r4,r5,r0,r1" );
	__asm( "STR     r5,[r2,#0]" );
	__asm( "STR     r4,[r3,#0]" );
	__asm( "LDMFD   sp!, {r4-r5}" );
}


/* Decodes character string b into a, where character string is ordered
   from most to least significant.

   Lengths: a[digits], b[len].
   Assumes b[i] = 0 for i < len - digits * MY_NN_DIGIT_LEN. (Otherwise most
   significant bytes are truncated.)
 */
void MY_NN_Decode (a, digits, b, len)
MY_NN_DIGIT *a;
unsigned char *b;
unsigned int digits, len;
{
  MY_NN_DIGIT t;
  int j;
  unsigned int i, u;

  for (i = 0, j = len - 1; i < digits && j >= 0; i++) {
    t = 0;
    for (u = 0; j >= 0 && u < MY_NN_DIGIT_BITS; j--, u += 8)
      t |= ((MY_NN_DIGIT)b[j]) << u;
    a[i] = t;
  }

  for (; i < digits; i++)
    a[i] = 0;
}

/* Encodes b into character string a, where character string is ordered
   from most to least significant.

   Lengths: a[len], b[digits].
   Assumes MY_NN_Bits (b, digits) <= 8 * len. (Otherwise most significant
   digits are truncated.)
 */
void MY_NN_Encode (a, len, b, digits)
MY_NN_DIGIT *b;
unsigned char *a;
unsigned int digits, len;
{
  MY_NN_DIGIT t;
  int j;
  unsigned int i, u;

  for (i = 0, j = len - 1; i < digits && j >= 0; i++) {
    t = b[i];
    for (u = 0; j >= 0 && u < MY_NN_DIGIT_BITS; j--, u += 8)
      a[j] = (unsigned char)(t >> u);
  }

  for (; j >= 0; j--)
    a[j] = 0;
}

/* Assigns a = b.

   Lengths: a[digits], b[digits].
 */
void MY_NN_Assign (a, b, digits)
MY_NN_DIGIT *a, *b;
unsigned int digits;
{
// change by liuxl for debug 20070612
	/*
  unsigned int i;

  for (i = 0; i < digits; i++)
    a[i] = b[i];
//	*/
	if(digits)
    {
        do
        {
            *a++ = *b++;
        }while(--digits);
    }
}

/* Assigns a = 0.

   Lengths: a[digits].
 */
void MY_NN_AssignZero (a, digits)
MY_NN_DIGIT *a;
unsigned int digits;
{
// change by liuxl for debug 20070612
	/*
  unsigned int i;

  for (i = 0; i < digits; i++)
    a[i] = 0;
//*/
  if(digits)
    {
        do
        {
            *a++ = 0;
        }while(--digits);
    }
}

/* Assigns a = 2^b.

   Lengths: a[digits].
   Requires b < digits * MY_NN_DIGIT_BITS.
 */
void MY_NN_Assign2Exp (a, b, digits)
MY_NN_DIGIT *a;
unsigned int b, digits;
{
  MY_NN_AssignZero (a, digits);

  if (b >= digits * MY_NN_DIGIT_BITS)
    return;

  a[b / MY_NN_DIGIT_BITS] = (MY_NN_DIGIT)1 << (b % MY_NN_DIGIT_BITS);
}

/* Computes a = b + c. Returns carry.

   Lengths: a[digits], b[digits], c[digits].
 */
MY_NN_DIGIT MY_NN_Add (a, b, c, digits)
MY_NN_DIGIT *a, *b, *c;
unsigned int digits;
{
  MY_NN_DIGIT ai, carry;
  unsigned int i;

  carry = 0;

  for (i = 0; i < digits; i++) {
    if ((ai = b[i] + carry) < carry)
      ai = c[i];
    else if ((ai += c[i]) < c[i])
      carry = 1;
    else
      carry = 0;
    a[i] = ai;
  }

  return (carry);
}

/* Computes a = b - c. Returns borrow.

   Lengths: a[digits], b[digits], c[digits].
 */
MY_NN_DIGIT MY_NN_Sub (a, b, c, digits)
MY_NN_DIGIT *a, *b, *c;
unsigned int digits;
{
  MY_NN_DIGIT ai, borrow;
  unsigned int i;

  borrow = 0;

  for (i = 0; i < digits; i++) {
    if ((ai = b[i] - borrow) > (MAX_MY_NN_DIGIT - borrow))
      ai = MAX_MY_NN_DIGIT - c[i];
    else if ((ai -= c[i]) > (MAX_MY_NN_DIGIT - c[i]))
      borrow = 1;
    else
      borrow = 0;
    a[i] = ai;
  }

  return (borrow);
}

/* Computes a = b * c.

   Lengths: a[2*digits], b[digits], c[digits].
   Assumes digits < MAX_MY_NN_DIGITS.
 */
void MY_NN_Mult (a, b, c, digits)
MY_NN_DIGIT *a, *b, *c;
unsigned int digits;
{
  MY_NN_DIGIT t[2*MAX_MY_NN_DIGITS];
//  unsigned int bDigits, cDigits, i; // for debug liuxl 20070612

  // add by liuxl for debug 20070612
    MY_NN_DIGIT dhigh, dlow, carry;
    MY_NN_DIGIT bDigits, cDigits, i, j;
	// add end

  MY_NN_AssignZero (t, 2 * digits);

  bDigits = MY_NN_Digits (b, digits);
  cDigits = MY_NN_Digits (c, digits);

  /* for debug liuxl 20070612
  for (i = 0; i < bDigits; i++)
    t[i+cDigits] += MY_NN_AddDigitMult (&t[i], &t[i], b[i], c, cDigits);
	//*/
  // add by liuxl for debug 20070612
   for (i = 0; i < bDigits; i++)
    {
        carry = 0;
        if(*(b+i) != 0)
        {
            for(j = 0; j < cDigits; j++)
            {
                dmult(*(b+i), *(c+j), &dhigh, &dlow);
                if((*(t+(i+j)) = *(t+(i+j)) + carry) < carry)
                    carry = 1;
                else
                    carry = 0;
                if((*(t+(i+j)) += dlow) < dlow)
                    carry++;
                carry += dhigh;
            }
        }
        *(t+(i+cDigits)) += carry;
    }
   // add end

  MY_NN_Assign (a, t, 2 * digits);

  /* Zeroize potentially sensitive information.
   */
  R_memset ((POINTER)t, 0, sizeof (t));
}

/* Computes a = b * 2^c (i.e., shifts left c bits), returning carry.

   Lengths: a[digits], b[digits].
   Requires c < MY_NN_DIGIT_BITS.
 */
MY_NN_DIGIT MY_NN_LShift (a, b, c, digits)
MY_NN_DIGIT *a, *b;
unsigned int c, digits;
{
  MY_NN_DIGIT bi, carry;
  unsigned int i, t;

  if (c >= MY_NN_DIGIT_BITS)
    return (0);

  t = MY_NN_DIGIT_BITS - c;

  carry = 0;

  for (i = 0; i < digits; i++) {
    bi = b[i];
    a[i] = (bi << c) | carry;
    carry = c ? (bi >> t) : 0;
  }

  return (carry);
}

/* Computes a = c div 2^c (i.e., shifts right c bits), returning carry.

   Lengths: a[digits], b[digits].
   Requires: c < MY_NN_DIGIT_BITS.
 */
MY_NN_DIGIT MY_NN_RShift (a, b, c, digits)
MY_NN_DIGIT *a, *b;
unsigned int c, digits;
{
  MY_NN_DIGIT bi, carry;
  int i;
  unsigned int t;

  if (c >= MY_NN_DIGIT_BITS)
    return (0);

  t = MY_NN_DIGIT_BITS - c;

  carry = 0;

  for (i = digits - 1; i >= 0; i--) {
    bi = b[i];
    a[i] = (bi >> c) | carry;
    carry = c ? (bi << t) : 0;
  }

  return (carry);
}

/* Computes a = c div d and b = c mod d.

   Lengths: a[cDigits], b[dDigits], c[cDigits], d[dDigits].
   Assumes d > 0, cDigits < 2 * MAX_MY_NN_DIGITS,
           dDigits < MAX_MY_NN_DIGITS.
 */
void MY_NN_Div (a, b, c, cDigits, d, dDigits)
MY_NN_DIGIT *a, *b, *c, *d;
unsigned int cDigits, dDigits;
{
  MY_NN_DIGIT ai, cc[2*MAX_MY_NN_DIGITS+1], dd[MAX_MY_NN_DIGITS]; //  t; // del for debug liuxl 20070612
  int i;
  unsigned int ddDigits, shift;
  // add by liuxl for debug 20070612
  MY_NN_DIGIT s;
    MY_NN_DIGIT t[2], u, v, *ccptr;
    MY_NN_HALF_DIGIT aHigh, aLow, cHigh, cLow;
	// add end

  ddDigits = MY_NN_Digits (d, dDigits);
  if (ddDigits == 0)
    return;

  /* Normalize operands.
   */
  shift = MY_NN_DIGIT_BITS - MY_NN_DigitBits (d[ddDigits-1]);
  MY_NN_AssignZero (cc, ddDigits);
  cc[cDigits] = MY_NN_LShift (cc, c, shift, cDigits);
  MY_NN_LShift (dd, d, shift, ddDigits);
  // changed by liuxl for debug 20070612
  // t = dd[ddDigits-1];
  s = dd[ddDigits-1];

  MY_NN_AssignZero (a, cDigits);

  /* for debug liuxl 20070612
  for (i = cDigits-ddDigits; i >= 0; i--) {
    // Underestimate quotient digit and subtract.

    if (t == MAX_MY_NN_DIGIT)
      ai = cc[i+ddDigits];
    else
      MY_NN_DigitDiv (&ai, &cc[i+ddDigits-1], t + 1);
    cc[i+ddDigits] -= MY_NN_SubDigitMult (&cc[i], &cc[i], ai, dd, ddDigits);

    // Correct estimate.

    while (cc[i+ddDigits] || (MY_NN_Cmp (&cc[i], dd, ddDigits) >= 0)) {
      ai++;
      cc[i+ddDigits] -= MY_NN_Sub (&cc[i], &cc[i], dd, ddDigits);
    }

    a[i] = ai;
  }
  //*/
  // add by liuxl for debug 20070612
    for (i = cDigits-ddDigits; i >= 0; i--)
    {
        if (s == MAX_MY_NN_DIGIT)
            ai = cc[i+ddDigits];
        else
        {
            ccptr = &cc[i+ddDigits-1];

            s++;
            cHigh = (MY_NN_HALF_DIGIT)HIGH_HALF(s);
            cLow = (MY_NN_HALF_DIGIT)LOW_HALF(s);

            *t = *ccptr;
            *(t+1) = *(ccptr+1);

            if (cHigh == MAX_MY_NN_HALF_DIGIT)
                aHigh = (MY_NN_HALF_DIGIT)HIGH_HALF(*(t+1));
            else
                aHigh = (MY_NN_HALF_DIGIT)(*(t+1) / (cHigh + 1));
            u = (MY_NN_DIGIT)aHigh * (MY_NN_DIGIT)cLow;
            v = (MY_NN_DIGIT)aHigh * (MY_NN_DIGIT)cHigh;
            if ((*t -= TO_HIGH_HALF(u)) > (MAX_MY_NN_DIGIT - TO_HIGH_HALF(u)))
                t[1]--;
            *(t+1) -= HIGH_HALF(u);
            *(t+1) -= v;

            while ((*(t+1) > cHigh) ||
                         ((*(t+1) == cHigh) && (*t >= TO_HIGH_HALF(cLow))))
            {
                if ((*t -= TO_HIGH_HALF(cLow)) > MAX_MY_NN_DIGIT - TO_HIGH_HALF(cLow))
                    t[1]--;
                *(t+1) -= cHigh;
                aHigh++;
            }

            if (cHigh == MAX_MY_NN_HALF_DIGIT)
                aLow = (MY_NN_HALF_DIGIT)LOW_HALF(*(t+1));
            else
                aLow = (MY_NN_HALF_DIGIT)((TO_HIGH_HALF(*(t+1)) + HIGH_HALF(*t)) / (cHigh + 1));
            u = (MY_NN_DIGIT)aLow * (MY_NN_DIGIT)cLow;
            v = (MY_NN_DIGIT)aLow * (MY_NN_DIGIT)cHigh;
            if ((*t -= u) > (MAX_MY_NN_DIGIT - u))
                t[1]--;
            if ((*t -= TO_HIGH_HALF(v)) > (MAX_MY_NN_DIGIT - TO_HIGH_HALF(v)))
                t[1]--;
            *(t+1) -= HIGH_HALF(v);

            while ((*(t+1) > 0) || ((*(t+1) == 0) && *t >= s))
            {
                if ((*t -= s) > (MAX_MY_NN_DIGIT - s))
                    t[1]--;
                aLow++;
            }

            ai = TO_HIGH_HALF(aHigh) + aLow;
            s--;
        }

        cc[i+ddDigits] -= subdigitmult(&cc[i], &cc[i], ai, dd, ddDigits);

        while (cc[i+ddDigits] || (MY_NN_Cmp(&cc[i], dd, ddDigits) >= 0))
        {
            ai++;
            cc[i+ddDigits] -= MY_NN_Sub(&cc[i], &cc[i], dd, ddDigits);
        }

        a[i] = ai;
    }

  /* Restore result.
   */
  MY_NN_AssignZero (b, dDigits);
  MY_NN_RShift (b, cc, shift, ddDigits);

  /* Zeroize potentially sensitive information.
   */
  R_memset ((POINTER)cc, 0, sizeof (cc));
  R_memset ((POINTER)dd, 0, sizeof (dd));
}

/* Computes a = b mod c.

   Lengths: a[cDigits], b[bDigits], c[cDigits].
   Assumes c > 0, bDigits < 2 * MAX_MY_NN_DIGITS, cDigits < MAX_MY_NN_DIGITS.
 */
void MY_NN_Mod (a, b, bDigits, c, cDigits)
MY_NN_DIGIT *a, *b, *c;
unsigned int bDigits, cDigits;
{
  MY_NN_DIGIT t[2 * MAX_MY_NN_DIGITS];

  MY_NN_Div (t, a, b, bDigits, c, cDigits);

  /* Zeroize potentially sensitive information.
   */
  R_memset ((POINTER)t, 0, sizeof (t));
}

/* Computes a = b * c mod d.

   Lengths: a[digits], b[digits], c[digits], d[digits].
   Assumes d > 0, digits < MAX_MY_NN_DIGITS.
 */
void MY_NN_ModMult (a, b, c, d, digits)
MY_NN_DIGIT *a, *b, *c, *d;
unsigned int digits;
{
  MY_NN_DIGIT t[2*MAX_MY_NN_DIGITS];

  MY_NN_Mult (t, b, c, digits);
  MY_NN_Mod (a, t, 2 * digits, d, digits);

  /* Zeroize potentially sensitive information.
   */
  R_memset ((POINTER)t, 0, sizeof (t));
}

/* Computes a = b^c mod d.

   Lengths: a[dDigits], b[dDigits], c[cDigits], d[dDigits].
   Assumes d > 0, cDigits > 0, dDigits < MAX_MY_NN_DIGITS.
 */
void MY_NN_ModExp (a, b, c, cDigits, d, dDigits)
MY_NN_DIGIT *a, *b, *c, *d;
unsigned int cDigits, dDigits;
{
  MY_NN_DIGIT bPower[3][MAX_MY_NN_DIGITS], ci, t[MAX_MY_NN_DIGITS];
  int i;
  unsigned int ciBits, j, s;

  /* Store b, b^2 mod d, and b^3 mod d.
   */
  MY_NN_Assign (bPower[0], b, dDigits);
  MY_NN_ModMult (bPower[1], bPower[0], b, d, dDigits);
  MY_NN_ModMult (bPower[2], bPower[1], b, d, dDigits);

  MY_NN_ASSIGN_DIGIT (t, 1, dDigits);

  cDigits = MY_NN_Digits (c, cDigits);
  for (i = cDigits - 1; i >= 0; i--) {
    ci = c[i];
    ciBits = MY_NN_DIGIT_BITS;

    /* Scan past leading zero bits of most significant digit.
     */
    if (i == (int)(cDigits - 1)) {
      while (! DIGIT_2MSB (ci)) {
        ci <<= 2;
        ciBits -= 2;
      }
    }

    for (j = 0; j < ciBits; j += 2, ci <<= 2) {
      /* Compute t = t^4 * b^s mod d, where s = two MSB's of ci.
       */
      MY_NN_ModMult (t, t, t, d, dDigits);
      MY_NN_ModMult (t, t, t, d, dDigits);
      if ((s = DIGIT_2MSB (ci)) != 0)
        MY_NN_ModMult (t, t, bPower[s-1], d, dDigits);
    }
  }

  MY_NN_Assign (a, t, dDigits);

  /* Zeroize potentially sensitive information.
   */

  R_memset ((POINTER)bPower, 0, sizeof (bPower));
  R_memset ((POINTER)t, 0, sizeof (t));

}

/* Compute a = 1/b mod c, assuming inverse exists.

   Lengths: a[digits], b[digits], c[digits].
   Assumes gcd (b, c) = 1, digits < MAX_MY_NN_DIGITS.
 */
void MY_NN_ModInv (a, b, c, digits)
MY_NN_DIGIT *a, *b, *c;
unsigned int digits;
{
  MY_NN_DIGIT q[MAX_MY_NN_DIGITS], t1[MAX_MY_NN_DIGITS], t3[MAX_MY_NN_DIGITS],
    u1[MAX_MY_NN_DIGITS], u3[MAX_MY_NN_DIGITS], v1[MAX_MY_NN_DIGITS],
    v3[MAX_MY_NN_DIGITS], w[2*MAX_MY_NN_DIGITS];
  int u1Sign;

  /* Apply extended Euclidean algorithm, modified to avoid negative
     numbers.
   */
  MY_NN_ASSIGN_DIGIT (u1, 1, digits);
  MY_NN_AssignZero (v1, digits);
  MY_NN_Assign (u3, b, digits);
  MY_NN_Assign (v3, c, digits);
  u1Sign = 1;

  while (! MY_NN_Zero (v3, digits)) {
    MY_NN_Div (q, t3, u3, digits, v3, digits);
    MY_NN_Mult (w, q, v1, digits);
    MY_NN_Add (t1, u1, w, digits);
    MY_NN_Assign (u1, v1, digits);
    MY_NN_Assign (v1, t1, digits);
    MY_NN_Assign (u3, v3, digits);
    MY_NN_Assign (v3, t3, digits);
    u1Sign = -u1Sign;
  }

  /* Negate result if sign is negative.
    */
  if (u1Sign < 0)
    MY_NN_Sub (a, c, u1, digits);
  else
    MY_NN_Assign (a, u1, digits);

  /* Zeroize potentially sensitive information.
   */
  R_memset ((POINTER)q, 0, sizeof (q));
  R_memset ((POINTER)t1, 0, sizeof (t1));
  R_memset ((POINTER)t3, 0, sizeof (t3));
  R_memset ((POINTER)u1, 0, sizeof (u1));
  R_memset ((POINTER)u3, 0, sizeof (u3));
  R_memset ((POINTER)v1, 0, sizeof (v1));
  R_memset ((POINTER)v3, 0, sizeof (v3));
  R_memset ((POINTER)w, 0, sizeof (w));
}

/* Computes a = gcd(b, c).

   Lengths: a[digits], b[digits], c[digits].
   Assumes b > c, digits < MAX_MY_NN_DIGITS.
 */
void MY_NN_Gcd (a, b, c, digits)
MY_NN_DIGIT *a, *b, *c;
unsigned int digits;
{
  MY_NN_DIGIT t[MAX_MY_NN_DIGITS], u[MAX_MY_NN_DIGITS], v[MAX_MY_NN_DIGITS];

  MY_NN_Assign (u, b, digits);
  MY_NN_Assign (v, c, digits);

  while (! MY_NN_Zero (v, digits)) {
    MY_NN_Mod (t, u, digits, v, digits);
    MY_NN_Assign (u, v, digits);
    MY_NN_Assign (v, t, digits);
  }

  MY_NN_Assign (a, u, digits);

  /* Zeroize potentially sensitive information.
   */
  R_memset ((POINTER)t, 0, sizeof (t));
  R_memset ((POINTER)u, 0, sizeof (u));
  R_memset ((POINTER)v, 0, sizeof (v));
}

/* Returns sign of a - b.

   Lengths: a[digits], b[digits].
 */
int MY_NN_Cmp (a, b, digits)
MY_NN_DIGIT *a, *b;
unsigned int digits;
{
  int i;

  for (i = digits - 1; i >= 0; i--) {
    if (a[i] > b[i])
      return (1);
    if (a[i] < b[i])
      return (-1);
  }

  return (0);
}

/* Returns nonzero iff a is zero.

   Lengths: a[digits].
 */
int MY_NN_Zero (a, digits)
MY_NN_DIGIT *a;
unsigned int digits;
{
  unsigned int i;

  for (i = 0; i < digits; i++)
    if (a[i])
      return (0);

  return (1);
}

/* Returns the significant length of a in bits.

   Lengths: a[digits].
 */
unsigned int MY_NN_Bits (a, digits)
MY_NN_DIGIT *a;
unsigned int digits;
{
  if ((digits = MY_NN_Digits (a, digits)) == 0)
    return (0);

  return ((digits - 1) * MY_NN_DIGIT_BITS + MY_NN_DigitBits (a[digits-1]));
}

/* Returns the significant length of a in digits.

   Lengths: a[digits].
 */
unsigned int MY_NN_Digits (a, digits)
MY_NN_DIGIT *a;
unsigned int digits;
{
  int i;

  for (i = digits - 1; i >= 0; i--)
    if (a[i])
      break;

  return (i + 1);
}

/* Computes a = b + c*d, where c is a digit. Returns carry.

   Lengths: a[digits], b[digits], d[digits].
 */
static MY_NN_DIGIT MY_NN_AddDigitMult (a, b, c, d, digits)
MY_NN_DIGIT *a, *b, c, *d;
unsigned int digits;
{
  MY_NN_DIGIT carry, t[2];
  unsigned int i;

  if (c == 0)
    return (0);

  carry = 0;
  for (i = 0; i < digits; i++) {
    MY_NN_DigitMult (t, c, d[i]);
    if ((a[i] = b[i] + carry) < carry)
      carry = 1;
    else
      carry = 0;
    if ((a[i] += t[0]) < t[0])
      carry++;
    carry += t[1];
  }

  return (carry);
}

/* Computes a = b - c*d, where c is a digit. Returns borrow.

   Lengths: a[digits], b[digits], d[digits].
 */
static MY_NN_DIGIT MY_NN_SubDigitMult (a, b, c, d, digits)
MY_NN_DIGIT *a, *b, c, *d;
unsigned int digits;
{
  MY_NN_DIGIT borrow, t[2];
  unsigned int i;

  if (c == 0)
    return (0);

  borrow = 0;
  for (i = 0; i < digits; i++) {
    MY_NN_DigitMult (t, c, d[i]);
    if ((a[i] = b[i] - borrow) > (MAX_MY_NN_DIGIT - borrow))
      borrow = 1;
    else
      borrow = 0;
    if ((a[i] -= t[0]) > (MAX_MY_NN_DIGIT - t[0]))
      borrow++;
    borrow += t[1];
  }

  return (borrow);
}

/* Returns the significant length of a in bits, where a is a digit.
 */
static unsigned int MY_NN_DigitBits (a)
MY_NN_DIGIT a;
{
  unsigned int i;

  for (i = 0; i < MY_NN_DIGIT_BITS; i++, a >>= 1)
    if (a == 0)
      break;

  return (i);
}


/****************************************************************************
  函数名     :
  描述       :
  输入参数   :
  输出参数   :
  返回值     :
  修改历史   :
      修改人     修改时间    修改版本号   修改原因
  1、
****************************************************************************/
MY_NN_DIGIT subdigitmult(MY_NN_DIGIT *a, MY_NN_DIGIT *b, MY_NN_DIGIT c, MY_NN_DIGIT *d, MY_NN_DIGIT digits)
{
    MY_NN_DIGIT borrow, thigh, tlow;
    MY_NN_DIGIT i;

    borrow = 0;

    if(c != 0)
    {
        for(i = 0; i < digits; i++)
        {
            dmult(c, d[i], &thigh, &tlow);
            if((a[i] = b[i] - borrow) > (MAX_MY_NN_DIGIT - borrow))
                borrow = 1;
            else
                borrow = 0;
            if((a[i] -= tlow) > (MAX_MY_NN_DIGIT - tlow))
                borrow++;
            borrow += thigh;
        }
    }

    return (borrow);
}


