#include "main.h"
#include "support.h"

void arm_flush_cache(uintptr_t addr, uint32_t length) {
    length = (length + 31) & ~31;
    while (length) {
        asm volatile("mcr p15, 0, %0, c7, c14, 1"::"r"(addr));
        addr += 32;
        length -= 32;
    }
    asm volatile("mcr p15, 0, %0, c7, c10, 4"::"r"(addr));
}

void arm_icache_invalidate(uintptr_t addr, uint32_t length) {
    length = (length + 31) & ~31;
    while (length) {
        asm volatile("mcr p15, 0, %0, c7, c5, 1"::"r"(addr));
        addr += 32;
        length -= 32;
    }
    asm volatile("mcr p15, 0, %0, c7, c10, 4"::"r"(addr));
}

void arm_dcache_invalidate(uintptr_t addr, uint32_t length) {
    length = (length + 31) & ~31;
    while (length) {
        asm volatile("mcr p15, 0, %0, c7, c6, 1"::"r"(addr));
        addr += 32;
        length -= 32;
    }
    asm volatile("mcr p15, 0, %0, c7, c10, 4"::"r"(addr));
}

const char *remove_path(const char *in) {
    const char *p = &in[strlen(in)-1];
    while (p > in && p[-1] != '/' && p[-1] != ':') p--;
    return p;
}

size_t strlen(const char *c) {
    size_t result = 0;
    while (*c++) result++;
    return result;
}

int strcmp(const char *s1, const char *s2) {
	while (*s1 == *s2++)
		if (*s1++ == '\0')
			return (0);
        
	return (*(const unsigned char *)s1 - *(const unsigned char *)(s2 - 1));
}


void *memmove(void *dst, const void *src, size_t sz) {
    uint8_t *d = dst;
    const uint8_t *s = src;

    if (d > s) {
        d += sz;
        s += sz;

        while(sz--)
            *--d = *--s;
    } else {
        while(sz--)
            *d++ = *s++;
    }

    return dst;
}

void memcpy(void *dst, const void *src, size_t sz)
{
    uint8_t *d = dst;
    const uint8_t *s = src;

    while(sz--)
	*d++ = *s++;
}

void memset(void *ptr, uint8_t fill, size_t sz)
{
    uint8_t *p = ptr;
    if (p)
        while(sz--)
            *p++ = fill;
}

void bzero(void *ptr, size_t sz)
{
    char *p = ptr;
    if (p)
        while(sz--)
            *p++ = 0;
}

