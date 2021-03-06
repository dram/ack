/*
 * $Source$
 * $State$
 */

#define	THREE_PASS          /* branch and offset optimization */
#define BYTES_REVERSED      /* high order byte has lowest address */
#define WORDS_REVERSED      /* high order word has lowest address */
#define LISTING             /* enable listing facilities */
#define RELOCATION          /* generate relocatable code */
#define DEBUG 0

#undef valu_t
#define valu_t long

#undef ADDR_T
#define ADDR_T long

#undef word_t
#define word_t long

typedef uint32_t quad;

#undef ALIGNWORD
#define ALIGNWORD	4

#undef ALIGNSECT
#define ALIGNSECT	4

#undef VALWIDTH
#define VALWIDTH	8

#define FIXUPFLAGS (RELBR | RELWR)
