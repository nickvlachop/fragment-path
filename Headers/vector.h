#ifndef Vector
#define Vector Vector
#include "DataStruct.h"
//Vectors (Dynamic arrays) are important for the data structure to access elements instantly
typedef struct Vector_str {
    void** head;
    uint32_t first;
    uint32_t last;
    uint32_t size;
    uint32_t count;
}Vector;
//The documentations for the functions are already in the linked list header
//The functionality is similar , the difference is in how those functions are written

void Vector_init(Vector* res);
//Transfers a Vector object from one memory block to an other , the old block of vector needs to initialized again to be usable , nothing is deallocated
void Vector_init_transfer(Vector* restrict dest, Vector* restrict src);
uint32_t Vector_count(Vector* restrict vector);
//Get the element in the specified index or NULL when no elements are present
void* Vector_get(Vector* restrict vector, uint32_t loc);
//Adds an element at the specified index , values greater than the number of elements inside the vector implies an append operation
void Vector_insert(Vector* restrict vector, uint32_t loc, createFunc creator , const void* obj);
//Executes a function at the element in the specified index , the function has to return the address of a new element or the address of the element or NULL to delete the element
void Vector_executeFunc(Vector* restrict vector, uint32_t loc, void* (*func)(void*, void*), void* args);
//Swaps the indexes of two elements
void Vector_swap(Vector* restrict vector, uint32_t pos1, uint32_t pos2);
//Clears the vector from every element within using the func argument
void Vector_clear(Vector* restrict vector, void* (*func)(void*, void*), void* args);
//Deallocates every memory the vector allocated using the func argument
void Vector_destroy(Vector* restrict vector, void* (*func)(void*, void*), void* args);
#endif // !Vector