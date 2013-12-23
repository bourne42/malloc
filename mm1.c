/*
 * mm.c
 *
 * Conrad Verkler conradv
 * solution to malloc lab 11/11
 *
 * Size stored in header and footer is the size of data, 
 * add 2 words to get the distance to the next block. 
 * All sizes in words
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mm.h"
#include "memlib.h"

/* If you want debugging output, use the following macro.  When you hand
 * in, remove the #define DEBUG line. */
#define DEBUG
#ifdef DEBUG
# define dbg_printf(...) printf(__VA_ARGS__)
#else
# define dbg_printf(...)
#endif


/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#endif /* def DRIVER */

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(p) (((size_t)(p) + (ALIGNMENT-1)) & ~0x7)


/***** My Stuff: *****/
void *createBlock(void *start, unsigned size, int alloc);
static int in_heap(const void *p);

#define START_SIZE (1<<8)
void *start=NULL;//Points to the start of our memory
unsigned totalSize=0;//amount of memory in bytes

/* given a pointer, returns the last bit of the byte it points to
 * used to store if a block is allocated or not
 */
#define is_alloc(p) ((*((unsigned *)(p))) & 0x01)
#define block_size(p) ( ((*((unsigned *)(p)))>>1))// & (0x7FFF))


void setHeader(void *p, unsigned s, unsigned b){
  unsigned *pi=p;
  *pi=((s<<1) | (b&0x01));
}
void setAlloc(void *p, unsigned b){
  setHeader(p, block_size(p), b);
}
void setBlock(void *p, unsigned s){
  setHeader(p, s, is_alloc(p));
}

/*
 * Initialize: return -1 on error, 0 on success.
 */
int mm_init(void) {
  start=mem_sbrk(START_SIZE);

  if(!start)
    return -1;
  totalSize=START_SIZE;
  //initialize begining block
  void *temp=createBlock(start, 0, 1);
  *((unsigned *)start)=(0<<1)|(1);
  printf("\n\nStart: %d => %d, %d\n", *(unsigned *)start, block_size(start), is_alloc(start));
  printf("SIZES: u: %lu, i: %lu\n, l:%lu", sizeof(unsigned), sizeof(int), sizeof(long int));
  //initialize middle block
  temp=createBlock(temp, (START_SIZE-6), 0);

  //initialize ending block
  createBlock(temp, 0, 1);

  printf("\n\nBAD: %d, %d, 1? %d\n", block_size(start), is_alloc(start), 
	 (*((unsigned *)start)==1));
  printf("Big block: %d, %d\n", block_size(start+2), is_alloc(start+2));


  return 0;
}

/* Declare the area in start to be a block, assumed all will fit.
 * Size is data size, will add 2 for total size of block. 
 * Returns a pointer to the next word after the end of this 
 * block for convenience. 
 */
void *createBlock(void *here, unsigned size, int alloc){
  setHeader(here, size, alloc);
  here+=(size+1);
  setHeader(here, size, alloc);
  return (here+1);
}

unsigned min(unsigned a, unsigned b){
  if(a<b)
    return a;
  return b;
}
unsigned max(unsigned a, unsigned b){
  if(a>b)
    return a;
  return b;
}

/* Given a pointer to the block this will allocate the data and return
 * a pointer to be returned by malloc. 
 * Assumes that the data can fit inside the block, will set alloc=1. 
 * Split if it can. 
 */
void *malloc_here(void *currentBlock, unsigned size){
  unsigned blockSize=block_size(currentBlock);
  unsigned newBlockSize;
  void *workingPtr=currentBlock;

  /* if there is space for a header,footer,and at least
   * another byte of data then split */
  if((blockSize-size-2)>0){
    newBlockSize=blockSize-size-2;
    
    workingPtr=createBlock(workingPtr, size, 1);
    createBlock(workingPtr, newBlockSize, 0);
    
    return (currentBlock+1);
  }
  else{
    setAlloc(currentBlock, 1);
    setAlloc(currentBlock+blockSize+1, 1);
    return (currentBlock+1);
  }//good size block
}

/*
 * malloc
 */
void *malloc (size_t size) {
  void *currentBlock=start+2;
  unsigned newSize=(ALIGN(size))<<3;//newSize is number of words (round up)
  unsigned sizeToAlloc;
  void *placeToAlloc;//only used if need more space

  /* Look at all available blocks: */
  while(block_size(currentBlock)!=0){
    unsigned blockSize=block_size(currentBlock);
    if((blockSize>=newSize) && is_alloc(currentBlock))//found space
      return malloc_here(currentBlock, newSize);
    else //too small
      currentBlock+=(blockSize+2);
  }

  /* No block will fit, add memory, currentBlock points to null block at end */
  sizeToAlloc=max(START_SIZE, newSize+2);
  if(!mem_sbrk(sizeToAlloc))
    return NULL;
  totalSize+=sizeToAlloc;
  placeToAlloc=currentBlock;

  /* set new block informatoin: */
  currentBlock=createBlock(currentBlock, (sizeToAlloc-2), 0);
  createBlock(currentBlock, 0, 1);

  return malloc_here(placeToAlloc, newSize);
}

/*
 * free
 */
void free (void *ptr) {
  unsigned oldS;//store original size that ptr points to
  unsigned neighS;//neighbor size
  void *nextPtr;//used for forwards just for ease of typing

  if(!ptr || !in_heap(ptr)) 
    return;

  ptr--;
  setAlloc(ptr, 0);
  setAlloc(ptr+(block_size(ptr)+1), 0);

  /* Coalesce Backwards: */
  if(!is_alloc((ptr-1))){
    oldS=block_size(ptr);
    neighS=block_size(ptr-1);
    ptr-=(neighS+2);
    createBlock(ptr, neighS+oldS+2, 0);
  }

  /* Coalesce Forwards: 
   *independent from backwards, just use ptr */
  nextPtr=ptr+block_size(ptr)+2;
  if(!is_alloc(nextPtr))
    createBlock(ptr, block_size(nextPtr)+2, 0);
}

/*
 * realloc - you may want to look at mm-naive.c
 */
void *realloc(void *oldptr, size_t size) {
  size++;
  oldptr=oldptr;
    return NULL;
}

/*
 * calloc - you may want to look at mm-naive.c
 * This function is not tested by mdriver, but it is
 * needed to run the traces.
 */
void *calloc (size_t nmemb, size_t size) {
  return malloc(nmemb*size);//still need to set all to 0
}

/*
 * Return whether the pointer is in the heap.
 * May be useful for debugging.
 */
static int in_heap(const void *p) {
    return p <= mem_heap_hi() && p >= mem_heap_lo();
}

/*
 * Return whether the pointer is aligned.
 * May be useful for debugging.
 */
static int aligned(const void *p) {
    return (size_t)ALIGN(p) == (size_t)p;
}

/*
 * mm_checkheap
 */
void mm_checkheap(int verbose) {
  void *currentBlock=start;

  printf("\nChecking Heap: verbose=%d\n", verbose);

  if(verbose==1){
    setHeader(currentBlock, 42,0);
    if(is_alloc(currentBlock))
      printf("ERROR: is_alloc didn't work\n");
    if(block_size(currentBlock)!=42)
      printf("ERROR: block_size didn't work\n");
    setHeader(currentBlock, 0, 1);
    if((block_size(currentBlock)!=0) || (!is_alloc(currentBlock)))
      printf("ERROR: is_alloc, block_size, or setHeader didn't work \n");
    printf("Tested setting and reading header\n");
  }//only test to see if reading size and alloc and setHeader works

  if(verbose>=2){
    if((*((unsigned *)currentBlock))!=1)
      printf("ERROR: Bad dummy starter header: %d => %d, %d\n", 
	     (*(unsigned *)currentBlock), 
	     block_size(currentBlock), is_alloc(currentBlock));
    currentBlock++;
    if((*((unsigned *)currentBlock))!=1)
      printf("ERROR: Bad dummy starter footer: %d => %d, %d\n", 
	     (*(unsigned *)currentBlock), 
	     block_size(currentBlock), is_alloc(currentBlock));
  }

  if(verbose>=3){
    while(block_size(currentBlock)!=0){
      if(!in_heap(currentBlock))
	printf("ERROR: Out of heap\n");
      if(!aligned(currentBlock))
	printf("ERROR: Not aligned\n");
      
      /* header and footer information is the same */
      if((*((unsigned *)currentBlock))!=
	 (*((unsigned *)(currentBlock+block_size(currentBlock)+1))))
	printf("ERROR: Header and Footer don't match up\n");
      
      /* if current block is not allocated make sure the next one is
       * only check inforward direction because every block checks
       */
      if(!(is_alloc(currentBlock) || 
	   is_alloc(currentBlock+block_size(currentBlock)+2)))
	printf("ERROR: coalescing fail\n");
    }//check all middle blocks
  }
  else
    currentBlock=start+totalSize-2;

  if(verbose>=2){
    if((*(unsigned *)currentBlock)!=1)
      printf("ERROR: Bad dummy finisher header: %d, %d\n", 
	     block_size(currentBlock), is_alloc(currentBlock));
    currentBlock++;
    if((*(unsigned *)currentBlock)!=1)
      printf("ERROR: Bad dummy finisher footer: %d, %d\n", 
	     block_size(currentBlock), is_alloc(currentBlock));
    if(mem_heap_hi()!=currentBlock)
      printf("ERROR: final block isn't heap high\n");
  }
  
  printf("Finished checking heap\n");
}
