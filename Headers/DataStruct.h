#ifndef DataStruct
#define DataStruct DataStruct
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
typedef void* restrict(*createFunc)(const void*);
//Deletes the cell using the function passed as argument and returns NULL
void* DS_nullify(void* cell, void* args);
struct DS_cpy_args { void* obj; uint32_t bytes; };
//This function allocates memory and copies an object to the allocated memory , using a pointer to a DS_cpy_args structure
void* DS_cpy(const void* args);
#endif