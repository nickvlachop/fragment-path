#ifndef DataStruct
#define DataStruct DataStruct
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
struct DS_nullify_args { void (*delfunc)(void*); };
struct DS_cpy_args { void* obj; uint32_t bytes; };
//These are helper functions to use for the data structures
typedef void* restrict(*createFunc)(const void*);
//This function returns NULL and deallocates memory of the provided element if the args argument is NULL or provided with a pointer to a DS_nullify_args structure it calls a delete function ,
//unless the delete function is NULL in which case the user needs to deallocate the memory.
void* DS_nullify(void* cell, void* args);
//This function allocates memory and copies an object to the allocated memory , using a pointer to a DS_cpy_args structure
void* DS_cpy(const void* args);
#endif