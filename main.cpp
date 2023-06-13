#include "VirtualMemory.h"

#include <cstdio>
#include <cassert>

int main (int argc, char **argv)
{
  VMinitialize ();
  int value = 0;
  VMwrite (13, 3);
  VMread (13,&value);
  printf("%d\n", value);

  VMwrite (6, 10);
  VMread (6,&value);
  printf("%d\n", value);




//  for (uint64_t i = 0; i < (2 * NUM_FRAMES); ++i)
//    {
//      printf ("writing to %llu\n", (long long int) i);
//      VMwrite (5 * i * PAGE_SIZE, i);
//    }
//
//  for (uint64_t i = 0; i < (2 * NUM_FRAMES); ++i)
//    {
//      word_t value;
//      VMread (5 * i * PAGE_SIZE, &value);
//      printf ("reading from %llu %d\n", (long long int) i, value);
//      assert(uint64_t(value) == i);
//    }
//  printf ("success\n");

  return 0;
}