/**
 * \file bn_mul.h
 */
/*
 *      Multiply source vector [s] with b, add result
 *       to destination vector [d] and set carry c.
 *
 *      Currently supports:
 *
 *         . IA-32 (386+)         . AMD64 / EM64T
 *         . IA-32 (SSE2)         . Motorola 68000
 *         . PowerPC, 32-bit      . MicroBlaze
 *         . PowerPC, 64-bit      . TriCore
 *         . SPARC v8             . ARM v3+
 *         . Alpha                . MIPS32
 *         . C, longlong          . C, generic
 */
#ifndef XYSSL_BN_MUL_H
#define XYSSL_BN_MUL_H

#if defined(__arm__)

#define MULADDC_INIT                            \
    asm( "ldr    r0, %0         " :: "m" (s));  \
    asm( "ldr    r1, %0         " :: "m" (d));  \
    asm( "ldr    r2, %0         " :: "m" (c));  \
    asm( "ldr    r3, %0         " :: "m" (b));

#define MULADDC_CORE                            \
    asm( "ldr    r4, [r0], #4   " );            \
    asm( "mov    r5, #0         " );            \
    asm( "ldr    r6, [r1]       " );            \
    asm( "umlal  r2, r5, r3, r4 " );            \
    asm( "adds   r7, r6, r2     " );            \
    asm( "adc    r2, r5, #0     " );            \
    asm( "str    r7, [r1], #4   " );

#define MULADDC_STOP                            \
    asm( "str    r2, %0         " : "=m" (c));  \
    asm( "str    r1, %0         " : "=m" (d));  \
    asm( "str    r0, %0         " : "=m" (s) :: \
    "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7" );

#endif /* ARMv3 */

#if defined(__mips__)

#define MULADDC_INIT                            \
    asm( "lw     $10, %0        " :: "m" (s));  \
    asm( "lw     $11, %0        " :: "m" (d));  \
    asm( "lw     $12, %0        " :: "m" (c));  \
    asm( "lw     $13, %0        " :: "m" (b));

#define MULADDC_CORE                            \
    asm( "lw     $14, 0($10)    " );            \
    asm( "multu  $13, $14       " );            \
    asm( "addi   $10, $10, 4    " );            \
    asm( "mflo   $14            " );            \
    asm( "mfhi   $9             " );            \
    asm( "addu   $14, $12, $14  " );            \
    asm( "lw     $15, 0($11)    " );            \
    asm( "sltu   $12, $14, $12  " );            \
    asm( "addu   $15, $14, $15  " );            \
    asm( "sltu   $14, $15, $14  " );            \
    asm( "addu   $12, $12, $9   " );            \
    asm( "sw     $15, 0($11)    " );            \
    asm( "addu   $12, $12, $14  " );            \
    asm( "addi   $11, $11, 4    " );

#define MULADDC_STOP                            \
    asm( "sw     $12, %0        " : "=m" (c));  \
    asm( "sw     $11, %0        " : "=m" (d));  \
    asm( "sw     $10, %0        " : "=m" (s) :: \
    "$9", "$10", "$11", "$12", "$13", "$14", "$15" );

#endif /* MIPS */

#endif /* bn_mul.h */
