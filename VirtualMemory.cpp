//
// Created by alexk on 09/06/2023.
//

#include "VirtualMemory.h"
#include "PhysicalMemory.h"

/*
 * According to this design, VMInitialize() only has to clear frame 0.
 */

void clearTable (uint64_t numTable)
{
  for (int row = 0; row < PAGE_SIZE; ++row)
    {
      // writing 0, for initializing the addresses in the table
      PMwrite (numTable * PAGE_SIZE + row, 0);

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
traverseTree (int currentDepth, uint64_t currentFrame, uint64_t maxFrameIndex,
              uint64_t currentVRAddress, uint64_t prevTable)
{
  if (currentFrame > maxFrameIndex)
    {
      maxFrameIndex = currentFrame;
    }
  if (isFrameEmpty(currentFrame))
    {
      return 0;//TODO: return the relevant address
    }
  if (currentDepth == TABLES_DEPTH)
    {
      return currentFrame;
    }

  for (int row = 0; row < PAGE_SIZE; ++row)
    {
      word_t nextFrame = 0;
      PMread (PAGE_SIZE * currentFrame + row, &nextFrame);
      auto rev_val = traverseTree (currentDepth + 1,
                                   nextFrame,
                                   maxFrameIndex, currentVRAddress,
                                   PAGE_SIZE * currentFrame + row);
    }
}

word_t findFreeFrame ()
{
  //TODO: not to evict the previous frame? or should i not evict even further?
  //TODO: traverse through the whole tree
  return 0;
}

uint64_t getAddress (uint64_t virtualAddress)
{
  uint64_t offSet = getOffSet (virtualAddress);
  virtualAddress = virtualAddress >> OFFSET_WIDTH; // TODO: is needed?
  uint64_t currentOffSet = 0;
  word_t nextToRead = 0;

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
          nextToRead = findFreeFrame ();
        }
    }
  //TODO: handle when missing page
  //TODO: implement the algorithm they describe via zeroing and so on
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
  auto readAdd = getAddress (virtualAddress);
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
  auto readAdd = getAddress (virtualAddress);
  PMwrite (readAdd, value);
  return 1;
}

