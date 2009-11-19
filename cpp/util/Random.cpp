/*
 * Copyright (c) 2008-2009 Digital Bazaar, Inc. All rights reserved.
 */
#include "db/util/Random.h"

#include "db/rt/System.h"

#include <stdlib.h>
#include <time.h>

using namespace db::rt;
using namespace db::util;

#ifdef WIN32
// windows rand() is shared amongst threads, and even seeding each individual
// thread will not change the sequence of numbers generated by each thread,
// that is, each thread will always generate the same sequence of random
// numbers -- therefore a simple public domain RNG is provided for windows --
// and it is intentionally not thread safe, allowing for more randomization
// of the seed value
static uint32_t gSeed;
#endif

void Random::seed()
{
   uint64_t value = System::getCurrentMilliseconds();
#ifdef WIN32
   gSeed = (uint32_t)(value & 0xFFFFFFFF) + time(NULL);
#else
   srandom((unsigned int)(value & 0xFFFFFFFF) + time(NULL));
#endif
}

uint64_t Random::next(uint64_t low, uint64_t high)
{
   // get a random number between low and high
   uint32_t r;

#ifdef WIN32
   /* Park-Miller "minimal standard" 31 bit PRNG, implemented with
    * David G. Carta's optimization: with 32 bit math and without
    * division (Public Domain).
    */
   uint32_t hi,lo;
   lo = 16807 * (gSeed & 0xFFFF);
   hi = 16807 * (gSeed >> 16);
   lo += (hi & 0x7FFF) << 16;
   lo += hi >> 15;
   lo = (lo & 0x7FFFFFFF) + (lo >> 31);
   r = gSeed = (uint32_t)lo;
   return low + (uint64_t)((high - low + 1) * r / (0x7FFFFFFF + 1.0));
#else
   r = random();
   return low + (uint64_t)((high - low + 1) * r / (RAND_MAX + 1.0));
#endif
}
