/*
 * mm.c
 *
 * Conrad Verkler conradv
 * solution to malloc lab 11/11
 *
 * Explicit list
 * Size stored in header and footer is the size of data, 
 * add 2 words to get the distance to the next block. 
 * Minimizes data stored to 2 workds (16 bytes) because needs
 * to be able to store pointers
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
void *createBlock(void *start, long size, long alloc);
void set1Ptr(void *block, void *ptr, int firstOrSecond);
void setPtrs(void *block, void *ptr1, void *ptr2);
void *getPtr(void *block, int firstOrSecond); 
static int in_heap(const void *p);
static int aligned(const void *p);

#define START_SIZE (1<<8)
void *start=NULL;//Points to the start of our memory
unsigned totalSize=0;//amount of memory in bytes
void *first;

/* given a pointer, returns the last bit of the byte it points to
 * used to store if a block is allocated or not
 */
#define gl(p) (*((long *)(p)))//return the long stored at p where p=void*
#define gp(p) (*((void **)(p)))//return the pointer (void *) stored at p
#define is_alloc(p) ((gl(p)) & 0x01)
#define block_size(p) ((gl(p))>>1)// & (0x7FFF FFFF))


void setHeader(void *p, long s, long b){
  long *pi=p;
  *pi=((s<<1) | (b&0x01));
}
void setAlloc(void *p, long b){
  setHeader(p, block_size(p), b);
}
void setBlock(void *p, long s){
  setHeader(p, s, is_alloc(p));
}
void testAlign(void *p){
  if(!aligned(p))
    printf("FOUND: Not Aligned: %p\n",p);
}

/*
 * Initialize: return -1 on error, 0 on success.
 */
int mm_init(void) {
  start=mem_sbrk(START_SIZE);
  //printf("\n\nStart: %p\n",start);

  if(!start)
    return -1;
  totalSize=START_SIZE;
  //initialize begining block
  void *temp=createBlock(start, 0, 1);

  //initialize middle block
  first=temp;
  setPtrs(temp, NULL, NULL);
  temp=createBlock(temp, (START_SIZE-(6*8)), 0);

  //initialize ending block
  createBlock(temp, 0, 1);
  mm_checkheap(3);
  printf("\n\nEverything initialized. First: %p\n", first);
  return 0;
}

/* Declare the area in start to be a block, assumed all will fit.
 * Size is data size, will add 2 for total size of block. 
 * Returns a pointer to the next word after the end of this 
 * block for convenience. 
 */
void *createBlock(void *here, long size, long alloc){
  setHeader(here, size, alloc);
  here+=(size+8);
  setHeader(here, size, alloc);
  return (here+8);
}
/* Set the pointer in a free block for the explicit list. 
 * firstOrSecond: give 1 to set the first pointer..., 
 * 1 is prev pointer, 2 is next pointer
 */
void set1Ptr(void *block, void *ptr, int firstOrSecond){
  void **newPtr=block;
  //    printf("\nPtr %p -> ",newPtr);
  newPtr+=firstOrSecond;
  //printf("%p\n", newPtr);
  *newPtr=ptr;

  /* set ptr's next/prev to block: */
  if(ptr!=NULL){
    int negate=(firstOrSecond==1)?2:1;
    newPtr=ptr;
    newPtr+=negate;
    *newPtr=block;
  }
}
void setPtrs(void *block, void *ptr1, void *ptr2){
  set1Ptr(block, ptr1, 1);
  set1Ptr(block, ptr2, 2);
}
void *getPtr(void *block, int firstOrSecond){
  return gp(block+(firstOrSecond*8));
}

unsigned min(long a, long b){
  if(a<b)
    return a;
  return b;
}
unsigned max(long a, long b){
  if(a>b)
    return a;
  return b;
}

/* Given a pointer to the block this will allocate the data and return
 * a pointer to be returned by malloc. 
 * Assumes that the data can fit inside the block, will set alloc=1. 
 * Split if it can. 
 */
void *malloc_here(void *currentBlock, long size){
  long blockSize=block_size(currentBlock);
  long newBlockSize;
  void *workingPtr=currentBlock;
  void *prev=getPtr(currentBlock, 1);
  void *next=getPtr(currentBlock, 2);
  printf("Working: %p\n", currentBlock);
  /* if there is space for a header,footer,and at least
   * 16 bytes (2 words) of data then split */
  if((blockSize-size-(2*8))>=(2*8)){
    newBlockSize=blockSize-size-(2*8);
    
    workingPtr=createBlock(workingPtr, newBlockSize, 0);
    createBlock(workingPtr, size, 1);
    printf("finished malloc\n");
    return (workingPtr+8);
  }
  else{
    setAlloc(currentBlock, 1);
    setAlloc(currentBlock+blockSize+8, 1);
    
    /* Set free linked info: */
    if(first==currentBlock){//invariant that prev=null
      if(next!=NULL)
	set1Ptr(next, NULL, 1);
      first=next;
    }
    else
      set1Ptr(prev, next, 2);//set prev's next pointer

    //if(next)//will be false at end of list
	//set1Ptr(next, prev, 1);//set next's prev pointer
    printf("finished malloc\n");
    return (currentBlock+8);
  }//good size block
}

/*
 * malloc
 */
void *malloc (size_t size) {
  printf("\nMALLOCING\n");
  void *currentBlock=first;
  long newSize=max((2*8),(ALIGN(size)));//newSize is actual size to use
  long sizeToAlloc;
  void *placeToAlloc;//only used if need more space
  //printf("\nStarting Malloc, request = %li\n", newSize);

  /* Look at all available blocks: */
  while(currentBlock!=NULL){
    long blockSize=block_size(currentBlock);
    if(is_alloc(currentBlock))
      printf("\nERROR: Bad free list found in malloc\n");
    if(blockSize>=newSize)//found space
      return malloc_here(currentBlock, newSize);
    else //too small
      currentBlock=getPtr(currentBlock, 1);
  }

  /* No block will fit, add memory, currentBlock points to null block at end */
  printf("Adding memory\n");
  sizeToAlloc=max(START_SIZE, newSize+(2*8));
  currentBlock=start+totalSize-(2*8);
  if(!mem_sbrk(sizeToAlloc))
    return NULL;
  totalSize+=sizeToAlloc;
  placeToAlloc=currentBlock;

  /* set first to be new block, its next is old first */
  setPtrs(currentBlock, NULL, first);
  first=currentBlock;

  /* set new block information: */
  currentBlock=createBlock(currentBlock, (sizeToAlloc-(2*8)), 0);
  createBlock(currentBlock, 0, 1);

  return malloc_here(placeToAlloc, newSize);
}

/*
 * free
 */
void free (void *ptr) {
  long oldS;//store original size that ptr points to
  long neighS;//neighbor size
  void *nextPtr;//used for forwards just for ease of typing
  //printf("\nStarting Free: %p\n", ptr);
  if(!ptr || !in_heap(ptr)) 
    return;

  ptr-=8;
  setAlloc(ptr, 0);
  setAlloc(ptr+(block_size(ptr)+8), 0);

  /* Coalesce Backwards: */
  if(!is_alloc((ptr-8))){
    oldS=block_size(ptr);
    neighS=block_size(ptr-8);
    ptr-=(neighS+(2*8));
    createBlock(ptr, neighS+oldS+(2*8), 0);
  }
  else{
    setPtrs(ptr, NULL, first);
    first=ptr;
  }

  /* Coalesce Forwards: 
   *independent from backwards, just use ptr */
  nextPtr=ptr+block_size(ptr)+(2*8);
  if(!is_alloc(nextPtr)){
    set1Ptr(getPtr(nextPtr,1), getPtr(nextPtr,2), 2);//set next's prt to the prevs
    createBlock(ptr, block_size(ptr)+block_size(nextPtr)+(2*8), 0);
  }
  
  //printf("Finished freeing\n");
}

/*
 * realloc - you may want to look at mm-naive.c
 */
void *realloc(void *oldptr, size_t size) {
  long oldSize;
  void *newptr;

  if(size==0){
    free(oldptr);
    return 0;
  }
  
  if(oldptr==NULL)
    return malloc(size);

  newptr=malloc(size);
  if(!newptr)
    return 0;

  oldSize=min(size, block_size(oldptr-8));

  memcpy(newptr, oldptr, oldSize);
  free(oldptr);
  return newptr;
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

  if(verbose>0)
    printf("\nChecking Heap: verbose=%d\n", verbose);

  /*if(verbose==1){
    setHeader(currentBlock, 42,0);
    if(is_alloc(currentBlock))
      printf("ERROR: is_alloc didn't work\n");
    if(block_size(currentBlock)!=42)
      printf("ERROR: block_size didn't work\n");
    setHeader(currentBlock, 0, 1);
    if((block_size(currentBlock)!=0) || (!is_alloc(currentBlock)))
      printf("ERROR: is_alloc, block_size, or setHeader didn't work \n");
    printf("Tested setting and reading header\n");
    }//only test to see if reading size and alloc and setHeader works*/

  if(verbose>=2){
    if(gl(currentBlock)!=1)
      printf("ERROR: Bad dummy starter header: %li => %li, %li\n", 
	     gl(currentBlock), 
	     block_size(currentBlock), is_alloc(currentBlock));
    currentBlock+=8;
    if(gl(currentBlock)!=1)
      printf("ERROR: Bad dummy starter footer: %li => %li, %li\n", 
	     gl(currentBlock), 
	     block_size(currentBlock), is_alloc(currentBlock));
  }

  if(verbose>=3){
    int freeCount=0;
    int totalBlockCount=0;
    currentBlock+=8;

    while(block_size(currentBlock)!=0){
      totalBlockCount++;
      if(!is_alloc(currentBlock))
	freeCount++;
      if(!in_heap(currentBlock))
	printf("ERROR: Out of heap\n");
      if(!aligned(currentBlock))
	printf("ERROR: Not aligned\n");
      //      if((((long)currentBlock)&0xFF)!=0)
      //printf("ERROR: misalign not found: %p\n", currentBlock);
      
      /* header and footer information is the same */
      if(gl(currentBlock)!=
	 gl(currentBlock+block_size(currentBlock)+8)){
	printf("ERROR: Header and Footer don't match up\n");
	break;
      }
      
      /* if current block is not allocated make sure the next one is
       * only check inforward direction because every block checks
       */
      if(!(is_alloc(currentBlock) || 
	   is_alloc(currentBlock+block_size(currentBlock)+(2*8))))
	printf("ERROR: coalescing fail\n");
      currentBlock+=block_size(currentBlock)+(2*8);
    }//check all middle blocks

    //    printf("Checked all blocks (%d total)\n", totalBlockCount);
    currentBlock=first;
    int freeInList=0;
    while(currentBlock!=NULL){
      freeInList++;
      void *next=getPtr(currentBlock, 2);//only check if next's prev points back here
      //because everyone is checked no need to check both ways, redudant
      if(next!=NULL && currentBlock==getPtr(next, 1))
	printf("ERROR: Linking doesn't match up\n");
      currentBlock=next;
    }
    if(freeCount!=freeInList)
      printf("ERROR: Lost a free block. Found: %d, Wanted: %d\n", 
	     freeInList, freeCount);

    printf("Checked all blocks (%d total)\n", totalBlockCount);
  }
 
  currentBlock=start+totalSize-(2*8);
  //  printf("Here: %p, end: %p\n", currentBlock, 
  if(verbose>=2){
    if(gl(currentBlock)!=1)
      printf("ERROR: Bad dummy finisher header: %li, %li\n", 
	     block_size(currentBlock), is_alloc(currentBlock));
    currentBlock+=8;
    if(gl(currentBlock)!=1)
      printf("ERROR: Bad dummy finisher footer: %li, %li\n", 
	     block_size(currentBlock), is_alloc(currentBlock));
    if(mem_heap_hi()!=(currentBlock+7))
      printf("ERROR: final block isn't heap high, found: %p, wanted: %p\n", 
	     (currentBlock+7), mem_heap_hi());
  }
  
  if(verbose>0)
    printf("Finished checking heap\n");
}
