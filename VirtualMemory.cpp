//
// Created by alexk on 09/06/2023.
//

#include "VirtualMemory.h"
#include "PhysicalMemory.h"
#include <cmath>

uint64_t getPhysicalAddress (uint64_t numFrame, uint64_t offset)
{
  return numFrame * PAGE_SIZE + offset;
}

bool isZeroed (uint64_t numFrame)
{
  word_t value = 0;
  for (int row = 0; row < PAGE_SIZE; ++row)
    {
      PMread (numFrame * PAGE_SIZE + row, &value);
      if (value != 0)
        {
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

/**
 * recursive DFS search
 */

/**
 *
 * @param virtualAddress
 * @param currentDepth
 * @param currentFrame
 * @param availableFrame for case 2
 * @param parentOfOldFrame the father of the frame that we are going to change
 * @param maxFrameIndex
 * @param maxCyclicDistance
 * @param to_evict
 * @param prevFrame of father that looking for the free frame
 * @param offsetParentOldFrame
 * @param isFoundEmptyFrame
 * @param currentFather father of the current junction
 * @param parentOffSet
 * @param savedFrame third case param, TODO: is needed?
 * @return
 */
word_t
traverseTree (uint64_t virtualAddress, int currentDepth, word_t currentFrame, uint64_t *availableFrame,
              word_t *parentOfOldFrame, uint64_t *maxFrameIndex,
              uint64_t *maxCyclicDistance, word_t *to_evict, word_t prevFrame,
              uint64_t *offsetParentOldFrame, bool *isFoundEmptyFrame,
              word_t *currentFather, uint64_t *parentOffSet, word_t *savedFrame)
{

  /** check if the right place is to check this stuff
   *
   */
  //TODO: watch out some danger here

  uint64_t currentStageOffSet = *parentOffSet;
  if (currentFrame > *maxFrameIndex)
    *maxFrameIndex = currentFrame;

  if (currentDepth == TABLES_DEPTH)
    {
      //TODO:
//      if(*availableFrame == 0)
//        return -1;

      uint64_t cyclic_distance = cyclicDistance (*availableFrame, virtualAddress);
      if (*maxCyclicDistance < cyclic_distance)
        {
          *maxCyclicDistance = cyclic_distance;
          *to_evict = *availableFrame;
          *savedFrame = currentFrame;
          *parentOfOldFrame = *currentFather;
          *offsetParentOldFrame = *parentOffSet;
        }
      *availableFrame = *availableFrame - currentStageOffSet;//TODO: watch out
      *availableFrame = *availableFrame >> OFFSET_WIDTH;
      return -1;
    }

  bool isEmptyFrame = true;
  for (uint64_t row = 0; row < PAGE_SIZE; ++row)
    {
      word_t nextFrame = 0;
      PMread (getPhysicalAddress (currentFrame, row), &nextFrame);

      if (nextFrame) // if nextFrame not 0
        {
          isEmptyFrame = false;
          *availableFrame = (uint64_t) (((*availableFrame) << OFFSET_WIDTH)
                                        + row);
          *parentOffSet = row;
          *currentFather = currentFrame;
          auto ret_val = traverseTree (virtualAddress, currentDepth
                                                       + 1, nextFrame, availableFrame, parentOfOldFrame, maxFrameIndex,
                                       maxCyclicDistance, to_evict, prevFrame, offsetParentOldFrame, isFoundEmptyFrame, currentFather, parentOffSet, savedFrame);

          if (ret_val != -1)
            {
              *availableFrame = *availableFrame - currentStageOffSet;//TODO: should be row?
              *availableFrame = *availableFrame >> OFFSET_WIDTH;
              return ret_val;
            }
        }
    }

  if (isEmptyFrame && currentFrame != prevFrame)
    {
      *availableFrame = *availableFrame - currentStageOffSet;
      *availableFrame = *availableFrame >> OFFSET_WIDTH;
      *isFoundEmptyFrame = true;
      return currentFrame;
    }
  *availableFrame = *availableFrame - currentStageOffSet;
  *availableFrame = *availableFrame >> OFFSET_WIDTH;
  return -1;
}

/**
 *
 * @param virtualAddress
 * @param currentDepth
 * @param prevFrame father of the new frame we are looking for
 * @param prevFrameOffSet
 * @return
 */
word_t
findFreeFrame (uint64_t virtualAddress, int currentDepth, word_t prevFrame, uint64_t prevFrameOffSet)
{
  uint64_t maxFrameIndex = 0;
  uint64_t maxCyclicDistance = 0;
  word_t to_evict = 0;// pathToLeaf = 0;
  uint64_t availableFrame = 0;
  word_t parentOfOldFrame = 0; // case 1 and 3
  uint64_t offsetParentOldFrame = 0;
  bool isFoundEmptyFrame = false;
  word_t currentFather = 0;
  uint64_t parentOffSet = 0;
  word_t savedFrame = 0;

  word_t frame = traverseTree (virtualAddress, 0, 0, &availableFrame,
                               &parentOfOldFrame, &maxFrameIndex,
                               &maxCyclicDistance, &to_evict, prevFrame,
                               &offsetParentOldFrame, &isFoundEmptyFrame,
                               &currentFather, &parentOffSet, &savedFrame);

  word_t frameToReturn = 0;
  if (isFoundEmptyFrame)
    {
      PMwrite (getPhysicalAddress (currentFather, parentOffSet), 0); // is parentOfOldFrame
      frameToReturn = frame;
    }
  else if (maxFrameIndex + 1 < NUM_FRAMES)
    {
//     if (currentDepth < TABLES_DEPTH - 1)//TODO: is needed?
//        clearTable (maxFrameIndex + 1);
      frameToReturn = maxFrameIndex + 1;
    }
  else
    {
      //parentOffSet wrong?
      PMwrite (getPhysicalAddress (parentOfOldFrame, offsetParentOldFrame), 0);
      PMevict (savedFrame, to_evict);
      frameToReturn = savedFrame;
    }

  PMwrite (getPhysicalAddress (prevFrame, prevFrameOffSet), frameToReturn);
  if (currentDepth == TABLES_DEPTH - 1)
    {
      PMrestore (frameToReturn, virtualAddress);
    }
  else
    {
      clearTable (frameToReturn);
    }
  return frameToReturn;
}

uint64_t getCurrentOffSet (uint64_t virtualAddress, int currentDepth)
{
  return (uint64_t) (
      virtualAddress >> (OFFSET_WIDTH * (TABLES_DEPTH - currentDepth - 1))
      &
      (PAGE_SIZE - 1));
}

uint64_t translateAddress (uint64_t virtualAddress)
{
  uint64_t finalOffSet = virtualAddress & (PAGE_SIZE - 1);
  virtualAddress = virtualAddress >> OFFSET_WIDTH;
  word_t currentFrame = 0;
  for (int currentDepth = 0; currentDepth < TABLES_DEPTH; ++currentDepth)
    {
      auto currentOffSet = getCurrentOffSet (virtualAddress, currentDepth);
      word_t prevFrame = currentFrame;
      PMread (getPhysicalAddress (currentFrame, currentOffSet), &currentFrame);
      if (!currentFrame) //currentFrame == 0 this address doesn't point to no- table
        {
          currentFrame = (word_t) findFreeFrame (virtualAddress, currentDepth, prevFrame, currentOffSet);
        }
    }

  return getPhysicalAddress ((uint64_t) currentFrame, finalOffSet);
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
  uint64_t offSet = virtualAddress & (PAGE_SIZE - 1);
//  auto page = translateAddress (virtualAddress >> OFFSET_WIDTH);
//  auto address = page * PAGE_SIZE + offSet;
  uint64_t address = translateAddress (virtualAddress);
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
  uint64_t offSet = virtualAddress & (PAGE_SIZE - 1);
  PMwrite (translateAddress (virtualAddress), value);
  return 1;
}
