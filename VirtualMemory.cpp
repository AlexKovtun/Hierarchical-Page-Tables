//
// Created by alexk on 09/06/2023.
//

#include "VirtualMemory.h"
#include "PhysicalMemory.h"
#include <cmath>
/*
 * According to this design, VMInitialize() only has to clear frame 0.
 */

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

bool isFrameEmpty (uint64_t baseIndex)
{
  uint64_t address = baseIndex * PAGE_SIZE;
  word_t val = 0;
  for (int row = 0; row < PAGE_SIZE; ++row)
    {
      PMread (address + row, &val);
      if (val != 0)
        {
          return false;
        }
    }
  return true;
}

/**
 * recursive DFS search
 */
uint64_t
traverseTree (int currentDepth, uint64_t currentFrame,uint64_t lastFrame, uint64_t *nextAvailableFrame,
              uint64_t prevAddress, uint64_t *maxDistance, uint64_t *maxPage, uint64_t *maxParent,
              uint64_t virtualAddress)
{
  if (currentFrame > *nextAvailableFrame)
    *nextAvailableFrame = currentFrame;

  if (currentDepth == TABLES_DEPTH)
    {
      uint64_t tmp = cyclicDistance (currentFrame, virtualAddress);
      if (*maxDistance < tmp)
        {
          *maxDistance = tmp;
          *maxPage = currentFrame;
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
                                       nextFrame,currentFrame,
                                       nextAvailableFrame,
                                       PAGE_SIZE * currentFrame
                                       + row, maxDistance,
                                       maxPage,
                                       maxParent,
                                       virtualAddress);
          if (ret_val != -1)
            return ret_val;
        }
    }

    if(isEmptyFrame && currentFrame != lastFrame){
        PMwrite (prevAddress, 0);
        return currentFrame;
    }
  return -1;
}

uint64_t findFreeFrame (uint64_t virtualAddress)
{
  uint64_t nextAvailableFrame = 0, maxDistance = 0, maxPage = 0, maxParent = 0;
  uint64_t address = traverseTree (0, 0,0,
                                   &nextAvailableFrame,
                                   0,
                                   &maxDistance, &maxPage, &maxParent,
                                   virtualAddress);
  if (address != -1)
    {
      return address;
    }

  else if (nextAvailableFrame + 1 < NUM_FRAMES)
    {
      clearTable (nextAvailableFrame + 1);
      return nextAvailableFrame + 1;
    }
//TODO: not to evict the previous frame? or should i not evict even further?
  PMevict (virtualAddress, maxPage);
  PMwrite (maxParent, 0);

  return 0;
}

uint64_t translateAddress (uint64_t virtualAddress)
{
  uint64_t offSet = getOffSet (virtualAddress);
  virtualAddress = virtualAddress >> OFFSET_WIDTH; // TODO: is needed?
  uint64_t currentOffSet = 0;
  word_t nextToRead = 0;
  bool needToBeRestored = false;

  //TODO: this should be generic
  for (int currentDepth = 0; currentDepth < TABLES_DEPTH; ++currentDepth)
    {
      currentOffSet = (uint64_t)
                          (virtualAddress >> OFFSET_WIDTH
                                             * (TABLES_DEPTH - currentDepth
                                                - 1))
                      & (PAGE_SIZE - 1);
      PMread (PAGE_SIZE * nextToRead + currentOffSet, &nextToRead);

      if (!nextToRead)
        { // we enter here iff we can't find empty frame
          uint64_t oldPage = nextToRead;
          nextToRead = (word_t) findFreeFrame (virtualAddress);
          PMwrite (oldPage * PAGE_SIZE + currentOffSet, nextToRead);
          needToBeRestored = true;
        }

    }
  if (needToBeRestored)
    PMrestore (nextToRead, virtualAddress);
  return nextToRead * PAGE_SIZE + offSet;
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
  auto readAdd = translateAddress (virtualAddress);
  PMread (readAdd, value);
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
  auto readAdd = translateAddress (virtualAddress);
  PMwrite (readAdd, value);
  return 1;
}

