#include <stdint.h>

#define MY_ARRAY ((volatile uint32_t *)0x80000000)

uint32_t fib(uint32_t n)
{
    if (n <= 1)
        return n;

    return fib(n - 1) + fib(n - 2);
}

int main(void)
{
    for (uint32_t i = 0; i < 16; ++i)
        MY_ARRAY[i] = fib(i);

    return 0;
}
