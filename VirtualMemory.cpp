//
// Created by alexk on 09/06/2023.
//

#include "VirtualMemory.h"
#include "PhysicalMemory.h"
#include <cmath>

bool isZeroed (uint64_t numFrame)
{
  word_t value = 0;
  for (int row = 0; row < PAGE_SIZE; ++row)
    {
      PMread( numFrame * PAGE_SIZE + row, &value);
      if(value != 0){
          return false;
        }
    }
  return true;
}

uint64_t translateAddress (uint64_t virtualAddress);
/**
 *
 * @param p
 * @param wantToInsert page_swapped_in
 * @return
 */
uint64_t cyclicDistance (uint64_t p, uint64_t wantToInsert)
{
  int64_t absDistance = std::abs ((int64_t) (wantToInsert - p));
  return (uint64_t) std::fmin (NUM_PAGES - absDistance, absDistance);
}

void clearTable (uint64_t numFrame)
{
  for (int row = 0; row < PAGE_SIZE; ++row)
    {
      // writing 0, for initializing the addresses in the table
      PMwrite (numFrame * PAGE_SIZE + row, 0);
    }
}


/*
 * According to this design, VMInitialize() only has to clear frame 0.
 */
void VMinitialize ()
{
  clearTable (0); // should it clear the whole table?
}

/** the access process
 *
 */

uint64_t getOffSet (uint64_t virtualAddress)
{
  uint64_t t = PAGE_SIZE - 1;
  return virtualAddress & t;
}

/**
 * recursive DFS search
 */
uint64_t
traverseTree (int currentDepth, uint64_t currentFrame, uint64_t lastFrame, uint64_t *nextAvailableFrame,
              uint64_t prevAddress, uint64_t *maxDistance, uint64_t *to_evict, uint64_t *maxParent, uint64_t pathToLeaf,
              uint64_t virtualAddress)
{

  if(isZeroed (currentFrame) && currentDepth < TABLES_DEPTH && currentFrame != lastFrame){
    return currentFrame;
  }

  if (currentFrame > *nextAvailableFrame)
    *nextAvailableFrame = currentFrame;


  if (currentDepth == TABLES_DEPTH)
    {
      uint64_t cyclic_distance = cyclicDistance (pathToLeaf, virtualAddress);
      if (*maxDistance < cyclic_distance && *to_evict != prevAddress)
        {
          *maxDistance = cyclic_distance;
          *to_evict = pathToLeaf;
          *maxParent = prevAddress;
        }
      return -1;
    }

  bool isEmptyFrame = true;
  for (int row = 0; row < PAGE_SIZE; ++row)
    {
      word_t nextFrame = 0;
      PMread (PAGE_SIZE * currentFrame + row, &nextFrame);

      if (nextFrame)
        {
          isEmptyFrame = false;
          auto ret_val = traverseTree (currentDepth + 1,
                                       nextFrame, lastFrame,
                                       nextAvailableFrame,
                                       PAGE_SIZE * currentFrame
                                       + row, maxDistance,
                                       to_evict,
                                       maxParent,
                                       (pathToLeaf << OFFSET_WIDTH) + row,
                                       virtualAddress);
          if (ret_val != -1)
            return ret_val;
        }
    }

  if (isEmptyFrame && currentFrame != lastFrame)
    {
      PMwrite (prevAddress, 0);
      return currentFrame;
    }
  return -1;
}

uint64_t findFreeFrame (uint64_t virtualAddress, uint64_t lastFrame, int currentDepth)
{
  uint64_t nextAvailableFrame = 0, maxDistance = 0, to_evict = 0, maxParent = 0;
  uint64_t pathToLeaf = 0;
  uint64_t address = traverseTree (0, 0, lastFrame,
                                   &nextAvailableFrame,
                                   0,
                                   &maxDistance, &to_evict, &maxParent,
                                   pathToLeaf,
                                   virtualAddress);
  if (address != -1)
    {
      return address;
    }

  else if (nextAvailableFrame + 1 < NUM_FRAMES)
    {
      if(currentDepth < TABLES_DEPTH-1)
        clearTable (nextAvailableFrame + 1);
      return nextAvailableFrame + 1;
    }

  uint64_t tmp = translateAddress (to_evict);
  PMwrite (maxParent, 0);
  PMevict (tmp, to_evict);
  if(currentDepth < TABLES_DEPTH-1)
    clearTable (to_evict);
  return tmp;
}


uint64_t translateAddress (uint64_t virtualAddress)
{
  //virtualAddress = virtualAddress >> OFFSET_WIDTH; // TODO: is needed? NOPE STUPID BUG!
  uint64_t lastFrame = 0;//last frame needed not to corrupt the last father table
  uint64_t prevFrame = 0;
  word_t nextToRead = 0;
  bool needToBeRestored = false;

  //TODO: this should be generic
  for (int currentDepth = 0; currentDepth < TABLES_DEPTH; ++currentDepth)
    {
      auto currentOffSet = (uint64_t) (
          virtualAddress >> (OFFSET_WIDTH * (TABLES_DEPTH - currentDepth - 1))
          &
          (PAGE_SIZE - 1));
      prevFrame = PAGE_SIZE * nextToRead + currentOffSet;
      PMread (prevFrame, &nextToRead);

      if (!nextToRead)
        { // we enter here iff we can't find empty frame
          nextToRead = (word_t) findFreeFrame (
              virtualAddress, lastFrame, currentDepth);
          PMwrite (prevFrame, nextToRead);
          lastFrame = nextToRead;
          needToBeRestored = true;
        }

    }
  if (needToBeRestored)
    PMrestore (nextToRead, virtualAddress);

  return nextToRead;
}

/* Reads a word from the given virtual address
 * and puts its content in *value.
 *
 * returns 1 on success.
 * returns 0 on failure (if the address cannot be mapped to a physical
 * address for any reason)
 */
int VMread (uint64_t virtualAddress, word_t *value)
{
  if (virtualAddress >= VIRTUAL_MEMORY_SIZE)
    {
      return 0;
    }
  uint64_t offSet = getOffSet (virtualAddress);
  auto page = translateAddress (virtualAddress>>OFFSET_WIDTH);
  auto address = page * PAGE_SIZE + offSet;
  PMread (address, value);
  return 1;
}

/* Writes a word to the given virtual address.
 *
 * returns 1 on success.
 * returns 0 on failure (if the address cannot be mapped to a physical
 * address for any reason)
 */
int VMwrite (uint64_t virtualAddress, word_t value)
{
  if (virtualAddress >= VIRTUAL_MEMORY_SIZE)
    {
      return 0;
    }
  uint64_t offSet = getOffSet (virtualAddress);
  auto page = translateAddress (virtualAddress>>OFFSET_WIDTH);
  auto address = page * PAGE_SIZE + offSet;
  PMwrite (address, value);
  return 1;
}
