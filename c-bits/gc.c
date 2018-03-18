#include <stdio.h>
#include <stdlib.h>
#include "gc.h"

#define HEAPPRINT 30

#define OFF       1
#define VInit     0x00000000
#define VMark     0x00000001
#define VFwd(A)   (A | VMark)

////////////////////////////////////////////////////////////////////////////////
// Imported from types.c
////////////////////////////////////////////////////////////////////////////////

extern int  is_number(int);
extern int  is_tuple(int);
extern int  tuple_at(int* base, int i);
extern int  tuple_size(int* base);
extern int* int_addr(int);
extern int  addr_int(int*);

////////////////////////////////////////////////////////////////////////////////

typedef struct Frame_ {
  int *sp;
  int *bp;
} Frame;

typedef enum Tag_
  { VAddr
  , VStackAddr
  , VNumber
  , VBoolean
  } Tag;

union Data
  { int* addr;
    int  value;
    int  gcvalue;
  };

typedef struct Value_
  { Tag        tag;
    union Data data;
  } Value;

////////////////////////////////////////////////////////////////////////////////
// Low-level API
////////////////////////////////////////////////////////////////////////////////

int valueInt(Value v){
  if (v.tag == VAddr || v.tag == VStackAddr) {
    return addr_int(v.data.addr);
  } else if (v.tag == VNumber){
    return (v.data.value << 1);
  } else { // v.tag == VBoolean
    return v.data.value;
  }
}

Value intValue(int v){
  Value res;
  if (is_tuple(v)) {
    res.tag       = VAddr;
    res.data.addr = int_addr(v);
  } else if (is_number(v)) {
    res.tag   = VNumber;
    res.data.value = v >> 1;
  } else {  // is_boolean(v)
    res.tag   = VBoolean;
    res.data.value = v;
  }
  return res;
}

Value getElem(int *addr, int i){
  int vi = tuple_at(addr, i);
  return intValue(vi);
}

void  setElem(int *addr, int i, Value v){
  addr[i+2] = valueInt(v);
}

void  setStack(int *addr, Value v){
  *addr = valueInt(v);
}

Value getStack(int* addr){
  return intValue(*addr);
}

int* extStackAddr(Value v){
  if (v.tag == VStackAddr)
    return v.data.addr;
  printf("GC-PANIC: extStackAddr");
  exit(1);
}

int* extHeapAddr(Value v){
  if (v.tag == VAddr)
    return v.data.addr;
  printf("GC-PANIC: extHeapAddr");
  exit(1);
}

void setSize(int *addr, int n){
  addr[0] = (n << 1);
}

int isLive(int *addr){
  return (addr[1] == VInit ? 0 : 1);
}

void  setGCWord(int* addr, int gv){
  if (DEBUG) fprintf(stderr, "\nsetGCWord: addr = %p, gv = %d\n", addr, gv);
  addr[1] = gv;
}

int*  forwardAddr(int* addr){
  return int_addr(addr[1]);
}

Value vHeapAddr(int* addr){
  return intValue(addr_int(addr));
}

int round_to_even(int n){
  return (n % 2 == 0) ? n : n + 1;
}

int blockSize(int *addr){
  int n = tuple_size(addr);
  return (round_to_even(n+2));

}
////////////////////////////////////////////////////////////////////////////////

Frame caller(int* stack_bottom, Frame frame){
  Frame callerFrame;
  int *bptr = frame.bp;
  if (bptr == stack_bottom){
    return frame;
  } else {
    callerFrame.sp = bptr + 1;
    callerFrame.bp = (int *) *bptr;
    return callerFrame;
  }
}

void print_stack(int* stack_top, int* first_frame, int* stack_bottom){
  Frame frame = {stack_top, first_frame };
  if (DEBUG) fprintf(stderr, "***** STACK: START sp=%p, bp=%p,bottom=%p *****\n", stack_top, first_frame, stack_bottom);
  do {
    if (DEBUG) fprintf(stderr, "***** FRAME: START *****\n");
    for (int *p = frame.sp; p < frame.bp; p++){
      if (DEBUG) fprintf(stderr, "  %p: %p\n", p, (int*)*p);
    }
    if (DEBUG) fprintf(stderr, "***** FRAME: END *****\n");
    frame    = caller(stack_bottom, frame);
  } while (frame.sp != stack_bottom);
  if (DEBUG) fprintf(stderr, "***** STACK: END *****\n");
}

void print_heap(int* heap, int size) {
  fprintf(stderr, "\n");
  for(int i = 0; i < size; i += 1) {
    fprintf(stderr
          , "  %d/%p: %p (%d)\n"
          , i
          , (heap + i)
          , (int*)(heap[i])
          , *(heap + i));
  }
}


////////////////////////////////////////////////////////////////////////////////
// FILL THIS IN, see documentation in 'gc.h' ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//TODO:: REMOVE WHILE(TRUE) HOW TO TELL WHEN DONE???
int* mark( int* stack_top
         , int* first_frame
         , int* stack_bottom
         , int* heap_start)
{
  // traverse from top to first frame
  int* heap_max = heap_start;
  while (stack_top != stack_bottom)
  {
    if(stack_top == first_frame)
    {
      first_frame = (int*) *first_frame;
      stack_top = stack_top + 2;
      continue;
    }
    int current = *stack_top;
    if ((current & 7) == 1) //is tuple. go to heap.
    {
      int * heap_current = (current - 1);
      markTuple(heap_current);
      if (heap_current > heap_max)
        heap_max = heap_current;
    }
    stack_top = stack_top + 1;
  }
  return heap_max;
}

void markTuple(int * tupleStart)
{
  int length = *tupleStart;
  tupleStart = tupleStart + 1;
  *tupleStart = 1;
  for (int i = 0; i < length; i ++)
  {
    tupleStart = tupleStart + 1;
    int temp = *tupleStart;
    if ((temp & 7) == 1)
      markTuple(temp - 1);
  }
}
//QUESTIONS: IS everything a tuple? heap start is guaranteed to be start of a tuple?
//While true in mark

////////////////////////////////////////////////////////////////////////////////
// FILL THIS IN, see documentation in 'gc.h' ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
int* forward( int* heap_start
            , int* max_address)
{
  //go through heap, setting gc word to compacted address. return address immediately following.
  int * current = heap_start;
  int * sendTo = heap_start;
  int length = 0;
  while (current <= max_address)
  {
    length = *current;
    current = current + 1;
    if(*current == 0)
    {
      current = current + (length + 1);
      continue;
    }
    else if (*current == 1)
    {
      *current = sendTo;        //CURRENTLY SETTING ADDRESS TO LENGTH CELL
      *current = *current + 1;
      sendTo = sendTo + (length + 2)
      current = current + (length + 1);
    }

  }
  return sendTo;
}

////////////////////////////////////////////////////////////////////////////////
// FILL THIS IN, see documentation in 'gc.h' ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void redirect( int* stack_bottom
             , int* stack_top
             , int* first_frame
             , int* heap_start
             , int* max_address )
{
  //Go through stack and update, thene go through heap and update.
  //STACK
  int current = *stack_top;
  int * currpointer = stack_top;
  int * currFrame = first_frame;
  int * heappointer;
  while(currpointer != stack_bottom)
  {
    //Check if = currFrame
    if(currpointer == currFrame)
    {
      currFrame = (int*) *currFrame;
      currpointer = currpointer + 2;
      continue;
    }
    current = *currpointer;
    if ((current & 7) != 1) //is not tuple. view next
    {
      currpointer = currpointer + 1;
      continue;
    }
    else  //is tuple: update address
    {
      heappointer = (int *) (current - 1);
      heappointer = heappointer + 1; //gc word - includes address to move to. 
      int newAddress = *heappointer;
      *currpointer = newAddress;
      currpointer = currpointer + 1;
    }
  }
  //HEAP
  currpointer = heap_start;
  while(currpointer <= max_address)
  {
    int length = *currpointer;
    currpointer = currpointer + 1; //gc word
    for (int i = 0; i < length; i++)
    {
      currpointer = currpointer + 1;
      //check if address
      int temp = *currpointer;
      if ((temp & 7) == 1) //is address
      {
        //look up address and update with the new
        int * tempptr = (int *) (*currpointer - 1);
        tempptr = tempptr + 1; //gc
        int newAddress = *tempptr;
        *currpointer = newAddress;

      }
    }
    currpointer = currpointer + 1;
  }
  return; 
}

////////////////////////////////////////////////////////////////////////////////
// FILL THIS IN, see documentation in 'gc.h' ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
void compact( int* heap_start
            , int* max_address
            , int* heap_end )
{ 
  //SET THE GC WORDS TO ZERO HERE
  int * current = heap_start;
  int * moveTo = heap_start;
  while (current <= max_address)
  {
    int length = *current;
    current = current + 1; //gc
    if (*current == 0)
    {
      current = current + (length + 1);
      continue;
    }
    *moveTo = length;
    moveTo = moveTo + 1;
    *moveTo = 0;
    moveTo = moveTo + 1;
    current = current + 1;
    for (int i = 0; i < length; i++)
    {
      *moveTo = *current;
      moveTo = moveTo + 1;
      current = current + 1;
    }

  }

  return;
}

////////////////////////////////////////////////////////////////////////////////
// Top-level GC function (you can leave this as is!) ///////////////////////////
////////////////////////////////////////////////////////////////////////////////

int* gc( int* stack_bottom
       , int* stack_top
       , int* first_frame
       , int* heap_start
       , int* heap_end )
{

  int* max_address = mark( stack_top
                         , first_frame
                         , stack_bottom
                         , heap_start );

  int* new_address = forward( heap_start
                            , max_address );

                     redirect( stack_bottom
                             , stack_top
                             , first_frame
                             , heap_start
                             , max_address );

                     compact( heap_start
                            , max_address
                            , heap_end );

  return new_address;
}
