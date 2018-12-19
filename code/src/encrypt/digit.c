/* DIGIT.C - digit arithmetic routines
 */

/* Copyright (C) RSA Laboratories, a division of RSA Data Security,
      Inc., created 1991. All rights reserved.
 */

#include "global.h"
#include "rsaref.h"
#include "nn.h"
#include "digit.h"

/* Computes a = b * c, where b and c are digits.

   Lengths: a[2].
 */
void MY_NN_DigitMult (a, b, c)
MY_NN_DIGIT a[2], b, c;
{
  MY_NN_DIGIT t, u;
  MY_NN_HALF_DIGIT bHigh, bLow, cHigh, cLow;

  bHigh = (MY_NN_HALF_DIGIT)HIGH_HALF (b);
  bLow = (MY_NN_HALF_DIGIT)LOW_HALF (b);
  cHigh = (MY_NN_HALF_DIGIT)HIGH_HALF (c);
  cLow = (MY_NN_HALF_DIGIT)LOW_HALF (c);

  a[0] = (MY_NN_DIGIT)bLow * (MY_NN_DIGIT)cLow;
  t = (MY_NN_DIGIT)bLow * (MY_NN_DIGIT)cHigh;
  u = (MY_NN_DIGIT)bHigh * (MY_NN_DIGIT)cLow;
  a[1] = (MY_NN_DIGIT)bHigh * (MY_NN_DIGIT)cHigh;

  if ((t += u) < u)
    a[1] += TO_HIGH_HALF (1);
  u = TO_HIGH_HALF (t);

  if ((a[0] += u) < u)
    a[1]++;
  a[1] += HIGH_HALF (t);
}

/* Sets a = b / c, where a and c are digits.

   Lengths: b[2].
   Assumes b[1] < c and HIGH_HALF (c) > 0. For efficiency, c should be
   normalized.
 */
void MY_NN_DigitDiv (a, b, c)
MY_NN_DIGIT *a, b[2], c;
{
  MY_NN_DIGIT t[2], u, v;
  MY_NN_HALF_DIGIT aHigh, aLow, cHigh, cLow;

  cHigh = (MY_NN_HALF_DIGIT)HIGH_HALF (c);
  cLow = (MY_NN_HALF_DIGIT)LOW_HALF (c);

  t[0] = b[0];
  t[1] = b[1];

  /* Underestimate high half of quotient and subtract.
   */
  if (cHigh == MAX_MY_NN_HALF_DIGIT)
    aHigh = (MY_NN_HALF_DIGIT)HIGH_HALF (t[1]);
  else
    aHigh = (MY_NN_HALF_DIGIT)(t[1] / (cHigh + 1));
  u = (MY_NN_DIGIT)aHigh * (MY_NN_DIGIT)cLow;
  v = (MY_NN_DIGIT)aHigh * (MY_NN_DIGIT)cHigh;
  if ((t[0] -= TO_HIGH_HALF (u)) > (MAX_MY_NN_DIGIT - TO_HIGH_HALF (u)))
    t[1]--;
  t[1] -= HIGH_HALF (u);
  t[1] -= v;

  /* Correct estimate.
   */
  while ((t[1] > cHigh) ||
         ((t[1] == cHigh) && (t[0] >= TO_HIGH_HALF (cLow)))) {
    if ((t[0] -= TO_HIGH_HALF (cLow)) > MAX_MY_NN_DIGIT - TO_HIGH_HALF (cLow))
      t[1]--;
    t[1] -= cHigh;
    aHigh++;
  }

  /* Underestimate low half of quotient and subtract.
   */
  if (cHigh == MAX_MY_NN_HALF_DIGIT)
    aLow = (MY_NN_HALF_DIGIT)LOW_HALF (t[1]);
  else
    aLow =
      (MY_NN_HALF_DIGIT)((TO_HIGH_HALF (t[1]) + HIGH_HALF (t[0])) / (cHigh + 1));
  u = (MY_NN_DIGIT)aLow * (MY_NN_DIGIT)cLow;
  v = (MY_NN_DIGIT)aLow * (MY_NN_DIGIT)cHigh;
  if ((t[0] -= u) > (MAX_MY_NN_DIGIT - u))
    t[1]--;
  if ((t[0] -= TO_HIGH_HALF (v)) > (MAX_MY_NN_DIGIT - TO_HIGH_HALF (v)))
    t[1]--;
  t[1] -= HIGH_HALF (v);

  /* Correct estimate.
   */
  while ((t[1] > 0) || ((t[1] == 0) && t[0] >= c)) {
    if ((t[0] -= c) > (MAX_MY_NN_DIGIT - c))
      t[1]--;
    aLow++;
  }

  *a = TO_HIGH_HALF (aHigh) + aLow;
}



