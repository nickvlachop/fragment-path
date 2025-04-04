#ifndef DataStruct
#define DataStruct DataStruct
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
typedef void* restrict(*createFunc)(const void*);
void* DS_nullify(void* cell, void* args);
//This function allocates memory and copies an object to the allocated memory , using a pointer to a DS_cpy_args structure
struct DS_cpy_args { void* obj; uint32_t bytes; };
void* DS_cpy(const void* args);
#endif