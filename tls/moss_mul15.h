/* The i15 limb products fit within 16 bits per input. On 68000,
 * complementary source operands pad MULU timing to a fixed total. */
#if !MOSS_TLS_MUL15_FALLBACK
static inline uint32_t moss_private_mul15(uint32_t x,uint32_t y) {
 uint32_t result,discard,source;
 __asm__ volatile("move.l %3,%0\n\tmove.l %3,%1\n\tmove.l %4,%2\n\tmulu.w %2,%0\n\tnot.w %2\n\tmulu.w %2,%1"
  : "=&d"(result),"=&d"(discard),"=&d"(source) : "d"(x),"d"(y) : "cc");
 return result;
}
#undef MUL15
#define MUL15(x,y) moss_private_mul15((uint32_t)(x),(uint32_t)(y))
#endif
