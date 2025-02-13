#ifndef LinkedList
#define LinkedList LinkedList
#include "DataStruct.h"
//A linked list which is used in Fragpath for various procedures where access speed is not significant but storing speed is important
typedef struct LinkedList_str {
    struct cell* head;
    struct cell* tail;
    uint32_t size;
}LinkedList;
//The linked list functions behave identical to the vector' functions
//Functions with index argument treat an out of bounds index as the index of the last element

void LinkedList_create(LinkedList* res);
//Adds an element at the specified index , values greater than the number of elements inside the linked list implies an append operation
void LinkedList_insert(LinkedList* restrict base, uint32_t loc, createFunc creator , const void* cont);
//Get the element in the specified index or NULL when no elements are present
void* LinkedList_get(LinkedList* restrict base, uint32_t loc);
//Executes a function at the element in the specified index , the function has to return the address of a new element or the address of the element or NULL to delete the element
void LinkedList_executeFunc(LinkedList* restrict base, uint32_t loc, void* (*func)(void*, void*), void* args);
//Swaps the indexes of two elements
void LinkedList_swap(LinkedList* restrict base, uint32_t location1, uint32_t location2);
//Returns the number of elements
uint32_t LinkedList_count(LinkedList* restrict base);
//Deallocates every memory it allocated using the func argument , but keeps the linked list usable
void LinkedList_clear(LinkedList* restrict base, void* (*func)(void*, void*), void* args);
//Deallocates every memory the linked list allocated using the func argument
void LinkedList_destroy(LinkedList* restrict base, void* (*func)(void*, void*), void* args);
#endif // !LinkedList