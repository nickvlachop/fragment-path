#ifndef Fragpath
#define Fragpath Fragpath
#include <stddef.h>
#include "DataStruct.h"
#include "vector.h"
#include "linkedlist.h"
#include "../version.h"

//The Fragpath works by sorting the keys using their bytes . The probelm is that compilers use padding bytes so that structures can be allingned at the memory, and a solution for this problem
//is to create a two dimensional array which contains the size and offset of every element.
//The following macros should help creating these arrays , but it is important that the last slot of the array has the size of the structure at the offset index and a zero value at the size index

typedef uint16_t seginfo[2];
#define Seginfo_Offset 0
#define Seginfo_Size 1

#define Seginfo_Primitive(x) {[0][Seginfo_Offset] = 0 , [0][Seginfo_Size] = sizeof(x) , [1][Seginfo_Offset] = sizeof(x) , [1][Seginfo_Size] = 0}
#define Seginfo_ElementInStruct(element,structure) {[Seginfo_Offset] = offsetof(structure,element) , [Seginfo_Size] = sizeof(((structure *)(0))->element)}
#define Seginfo_EndOfStruct(structure) {[Seginfo_Offset] = sizeof(structure) , [Seginfo_Size] = 0}

typedef struct Fragpath_str {
    Vector array;
#ifdef MultiThread
    uint8_t thrd_active_count;
    uint8_t writercount;
    uint8_t readercount;
#endif
    uint16_t largestByte;
    uint16_t bytes;
    struct instruction* objStructSizes;
}Fragpath;

//Creates the Fragpath
void Fragpath_create(Fragpath*, const seginfo* restrict objectStruct, uint8_t arrayCount);
//Adds a key to the Fragpath , does nothing if the key exists
void Fragpath_insert(Fragpath*, const void* key , createFunc creator , const void* newObj);
//Gets the value from a key , returns NULL if they key does not exists
void* Fragpath_contains(Fragpath*, const void* key);
//Executes a function at a value specified by the key, the function has to return the address of a new value or the address of the current value or NULL to delete the key
void Fragpath_executeFunc(Fragpath*, const void* key , void* (*func)(void*, void*), void* args);
//Swaps the values of two keys
void Fragpath_swap(Fragpath*, const void* key1, const void* key2);
//Returns an array of every key within the Fragpath
void* Fragpath_getKeys(Fragpath*, uint32_t* keyCount);
//Frees every resourse the Fragpath allocated and deletes every value stored within the Fragpath using the func argument
void Fragpath_destroy(Fragpath* , void* (*func)(void*, void*), void* args);
//Deletes every key and value within the Fragpath using the func argument
void Fragpath_clear(Fragpath* , void* (*func)(void*, void*), void* args);
#endif // !Fragpath