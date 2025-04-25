#include "../Headers/linkedlist.h"
#include <stddef.h>
struct cell{
    struct cell * restrict next;
    struct cell * restrict previous;
    void* restrict context;
};

static struct cell * parseCell(LinkedList* restrict base,uint32_t location){
    uint32_t tailDistance = (base->size - 1) - location;
    register uint32_t steps;
    register struct cell* position;
    register uint8_t offset;
    if (tailDistance < location) {
        position = base->tail;
        steps = tailDistance;
        offset = offsetof(struct cell, previous);
    }
    else {
        position = base->head;
        steps = location;
        offset = offsetof(struct cell, next);
    }
    while (steps) { position = *(struct cell**)((char*)position + offset); steps -= 1; }
    return position;
}

void LinkedList_insert(LinkedList* restrict base, uint32_t loc , void * restrict content){
    struct cell* newCell = (struct cell*)malloc(sizeof(struct cell));
    newCell->context = content;
    if (base->size == 0) {
        base->head = base->tail = newCell;
        newCell->next = NULL;
        newCell->previous = NULL;
    }
    else {
        if (loc == 0) {
            newCell->next = base->head;
            newCell->previous = NULL;
            base->head->previous = newCell;
            base->head = newCell;
        }
        else if (loc >= base->size) {
            newCell->previous = base->tail;
            newCell->next = NULL;
            base->tail->next = newCell;
            base->tail = newCell;
        }
        else {
            struct cell* place = parseCell(base, loc - 1);
            struct cell* placeNext = place->next;
            place->next = newCell;
            newCell->previous = place;
            newCell->next = placeNext;
            placeNext->previous = newCell;
        }
    }
    base->size += 1;
    return;
}

void * restrict LinkedList_get(LinkedList* restrict base,uint32_t loc){
    void* restrict result;
    if (base->size == 0)result = NULL;
    else if (loc >= base->size)result = base->tail->context;
    else result = parseCell(base, loc)->context;
    return result;
}
static void LinkedList_remove(LinkedList* restrict base, uint32_t loc, struct cell * restrict place) {
    switch (base->size) {
    default:
        if (loc == 0) {
            place->next->previous = NULL;
            base->head = place->next;
        }
        else if (loc >= (base->size - 1)) {
            place->previous->next = NULL;
            base->tail = place->previous;
        }
        else {
            place->previous->next = place->next;
            place->next->previous = place->previous;
        }
        break;
    case 1:
        base->head = base->tail = NULL;
        break;
    case 2:
        if (loc == 0) {
            base->head = base->tail;
            base->tail->previous = NULL;
        }
        else {
            base->tail = base->head;
            base->head->next = NULL;
        }
        break;
    }
    free(place);
    base->size -= 1;
    return;
}

void LinkedList_executeFunc(LinkedList* restrict base, uint32_t loc, void* (*func)(void*, void*), void* args) {
    struct cell* place;
    if (base->size == 0)place = NULL;
    else if (loc >= base->size)place = base->tail;
    else place = parseCell(base, loc);
    if ((place->context = func(place->context, args)) == NULL)
        LinkedList_remove(base, loc ,place);
    return;
}

void LinkedList_delete(LinkedList* restrict base, uint32_t loc, void (*func)(void*)) {
    struct cell* place;
    if (base->size == 0)place = NULL;
    else if (loc >= base->size)place = base->tail;
    else place = parseCell(base, loc);
    func(place->context);
    LinkedList_remove(base, loc, place);
    return;
}

void LinkedList_swap(LinkedList* restrict base,uint32_t location1,uint32_t location2){
    uint32_t loc1, loc2 , diff;
    if (location1 > location2){
        loc1 = (-(location1 >= base->size) & ((base->size - 1)) - location1) + location1;
        loc2 = (-(location2 >= base->size) & ((base->size - 1)) - location2) + location2;
    }
    else{
        loc1 = (-(location2 >= base->size) & ((base->size - 1)) - location2) + location2;
        loc2 = (-(location1 >= base->size) & ((base->size - 1)) - location1) + location1;
    }
    diff = loc1 - loc2;
    if(diff){
        struct cell* ob1, * ob2;
        ob1 = parseCell(base, loc1);
        ob2 = parseCell(base, loc2);
        struct cell *ob1_Old_Prev = ob1->previous, *ob1_Old_Next = ob1->next, *ob2_Old_Prev = ob2->previous, *ob2_Old_Next = ob2->next;
        ob1->previous = ob2_Old_Prev;
        ob2->next = ob1_Old_Next;
        if (diff == 1) {
            ob1->next = ob2;
            ob2->previous = ob1;
        }
        else {
            ob1->next = ob2_Old_Next;
            ob2->previous = ob1_Old_Prev;
            ob2_Old_Next->previous = ob1;
            ob1_Old_Prev->next = ob2;
        }
        if (ob2_Old_Prev)ob2_Old_Prev->next = ob1;
        else base->head = ob1;
        if (ob1_Old_Next)ob1_Old_Next->previous = ob2;
        else base->tail = ob2;
    }
    return;
}
uint32_t LinkedList_count(LinkedList* restrict base){
    return base->size;
}
void LinkedList_init(LinkedList* res){
    res->head = NULL;
    res->tail = NULL;
    res->size = 0;
}
void LinkedList_clear(LinkedList* restrict base, void (*func)(void*)){
    LinkedList_destroy(base,func);
    base->head = NULL;
    base->tail = NULL;
    base->size = 0;
    return;
}

void LinkedList_destroy(LinkedList* restrict base, void (*func)(void*)){
    struct cell* position = base->head , *next;
    if(func) while (position != NULL) {
        next = position->next;
        func(position->context);
        free(position);
        position = next;
    }
    else {
        next = position->next;
        free(position);
        position = next;
    }
    return;
}