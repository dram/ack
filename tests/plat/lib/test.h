#ifndef TEST_H
#define TEST_H

#include <unistd.h>
#include <stdint.h>

extern void finished(void);
extern void writehex(uint32_t code);
extern void fail(uint32_t code);

#define ASSERT(condition) \
    do { if (!(condition)) fail(__LINE__); } while(0)

#endif
