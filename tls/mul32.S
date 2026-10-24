/* cdecl with 32-bit arguments. Fixed work, low 32 bits of x*y.
 * Complementary source MULU pairs cancel 68000 popcount-dependent timing. */
.text
.even
.globl moss_private_mul15
moss_private_mul15:
 movem.l %d2-%d4,-(%sp)
 move.l 16(%sp),%d0
 move.l 20(%sp),%d1
 move.l %d0,%d4
 mulu.w %d1,%d0
 not.w %d1
 mulu.w %d1,%d4
 not.w %d1
 move.l 16(%sp),%d2
 swap %d2
 move.l %d2,%d4
 mulu.w %d1,%d2
 not.w %d1
 mulu.w %d1,%d4
 not.w %d1
 move.l 16(%sp),%d3
 swap %d1
 move.l %d3,%d4
 mulu.w %d1,%d3
 not.w %d1
 mulu.w %d1,%d4
 add.l %d3,%d2
 swap %d2
 clr.w %d2
 add.l %d2,%d0
 movem.l (%sp)+,%d2-%d4
 rts
