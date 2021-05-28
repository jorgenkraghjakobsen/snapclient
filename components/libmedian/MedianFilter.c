/*
 * MedianFilter.c
 *
 *  Created on: May 19, 2018
 *      Author: alexandru.bogdan
 *      Editor: Carlos Derseher
 *
 *      original source code:
 *      https://github.com/accabog/MedianFilter
 */

/**
 * This Module expects odd numbers of buffer lengths!!!
 */

#include <stdint.h>

#include "MedianFilter.h"

/**
 *
 */
int MEDIANFILTER_Init(sMedianFilter_t *medianFilter) {
  if (medianFilter && medianFilter->medianBuffer &&
      (medianFilter->numNodes % 2) && (medianFilter->numNodes > 1)) {
    // initialize buffer nodes
    for (unsigned int i = 0; i < medianFilter->numNodes; i++) {
      medianFilter->medianBuffer[i].value = 0;
      medianFilter->medianBuffer[i].nextAge =
          &medianFilter->medianBuffer[(i + 1) % medianFilter->numNodes];
      medianFilter->medianBuffer[i].nextValue =
          &medianFilter->medianBuffer[(i + 1) % medianFilter->numNodes];
      medianFilter->medianBuffer[(i + 1) % medianFilter->numNodes].prevValue =
          &medianFilter->medianBuffer[i];
    }
    // initialize heads
    medianFilter->ageHead = medianFilter->medianBuffer;
    medianFilter->valueHead = medianFilter->medianBuffer;
    medianFilter->medianHead =
        &medianFilter->medianBuffer[medianFilter->numNodes / 2];

    medianFilter->bufferCnt = 0;

    return 0;
  }

  return -1;
}

/**
 *
 */
int64_t MEDIANFILTER_Insert(sMedianFilter_t *medianFilter, int64_t sample) {
  unsigned int i;
  sMedianNode_t *newNode, *it;

  if (medianFilter->ageHead ==
      medianFilter->valueHead) {  // if oldest node is also the smallest node,
                                  // increment value head
    medianFilter->valueHead = medianFilter->valueHead->nextValue;
  }

  if ((medianFilter->ageHead == medianFilter->medianHead) ||
      (medianFilter->ageHead->value >
       medianFilter->medianHead->value)) {  // prepare for median correction
    medianFilter->medianHead = medianFilter->medianHead->prevValue;
  }

  // replace age head with new sample
  newNode = medianFilter->ageHead;
  newNode->value = sample;

  // remove age head from list
  medianFilter->ageHead->nextValue->prevValue =
      medianFilter->ageHead->prevValue;
  medianFilter->ageHead->prevValue->nextValue =
      medianFilter->ageHead->nextValue;
  // increment age head
  medianFilter->ageHead = medianFilter->ageHead->nextAge;

  // find new node position
  it = medianFilter->valueHead;  // set iterator as value head
  for (i = 0; i < medianFilter->numNodes - 1; i++) {
    if (sample < it->value) {
      if (i == 0) {  // replace value head if new node is the smallest
        medianFilter->valueHead = newNode;
      }
      break;
    }
    it = it->nextValue;
  }

  // insert new node in list
  it->prevValue->nextValue = newNode;
  newNode->prevValue = it->prevValue;
  it->prevValue = newNode;
  newNode->nextValue = it;

  if (medianFilter->bufferCnt < medianFilter->numNodes) {
    medianFilter->bufferCnt++;
  }

  // adjust median node
  if (i >= (medianFilter->numNodes / 2)) {
    medianFilter->medianHead = medianFilter->medianHead->nextValue;
  }

  return medianFilter->medianHead->value;
}

/**
 *
 */
uint8_t MEDIANFILTER_isFull(sMedianFilter_t *medianFilter) {
  if (medianFilter->bufferCnt < medianFilter->numNodes) {
    return 0;
  } else {
    return 1;
  }
}
