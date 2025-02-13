#include "../Headers/vector.h"

static void refactor(Vector* restrict vector, uint32_t newSize) {
    register uint32_t i = 0;
    register void** mainLoc;
    register uint32_t count;
    if (newSize < vector->size) {
        register void** newLoc = vector->head;
        register uint32_t sizeDiff = vector->first;
        mainLoc = newLoc + sizeDiff;
        if (sizeDiff > vector->last) {
            count = vector->size - sizeDiff;
            sizeDiff = vector->size - newSize;
            newLoc = mainLoc - sizeDiff;
            for (; i < count; ++i)newLoc[i] = mainLoc[i];
            vector->first -= sizeDiff;
        }
        else {
            count = vector->count;
            if (newSize < vector->last) {
                for (; i < count; ++i)newLoc[i] = mainLoc[i];
                vector->first = 0;
                vector->last = count;
            }
            else if (vector->last == newSize)vector->last = 0;
        }
        vector->head = (void**)realloc(vector->head, newSize * sizeof(void**));
    }
    else {
        vector->head = mainLoc = (void**)realloc(vector->head, newSize * sizeof(void**));
        count = vector->first;
        register uint32_t tempDest = newSize;
        register uint32_t counter = vector->count;
        if (count <= (counter >> 1)) {
            for (; i < count; i++) {
                mainLoc[counter % tempDest] = mainLoc[i];
                counter += 1;
            }
            vector->last = counter % tempDest;
        }
        else {
            for (i = counter - 1; i >= count; i--) {
                tempDest = tempDest - 1;
                mainLoc[tempDest] = mainLoc[i];
            }
            vector->first = tempDest;
        }
    }
    vector->size = newSize;
}

uint32_t Vector_count(Vector* restrict vector){
    return vector->count;
}
void * Vector_get(Vector* restrict vector,uint32_t loc){
    void* result;
    if (vector->count == 0) result = NULL;
    else if (loc >= vector->count) result = vector->head[(vector->size + vector->last - 1) % vector->size];
    else result = vector->head[(vector->first + loc) % vector->size];
    return result;
}

void Vector_insert(Vector* restrict vector, uint32_t loc , createFunc creator , const void* obj) {
    register uint32_t cSize = vector->size;
    register uint32_t count = vector->count;
    if (count == cSize) refactor(vector, (cSize = cSize << 1));
    register void** bufferHead = vector->head;
    register uint32_t actualPos;
    if (loc <= ((count + 1) >> 1)) {
        bufferHead[(vector->first = actualPos = ((cSize + vector->first) - 1) % cSize)] = creator(obj);
        loc = (vector->first + loc) % cSize;
        while (actualPos != loc) {
            register uint32_t newPos = (actualPos + 1) % cSize;
            register void* cont = bufferHead[actualPos];
            bufferHead[actualPos] = bufferHead[newPos];
            bufferHead[newPos] = cont;
            actualPos = newPos;
        }
    }
    else {
        bufferHead[(actualPos = vector->last)] = creator(obj);
        vector->last = (actualPos + 1) % cSize;
        loc = (vector->first + loc) % cSize;
        while (actualPos != loc) {
            register uint32_t newPos = ((cSize + actualPos) - 1) % cSize;
            register void* cont = bufferHead[actualPos];
            bufferHead[actualPos] = bufferHead[newPos];
            bufferHead[newPos] = cont;
            actualPos = newPos;
        }
    }
    vector->count = count + 1;
    return;
}

void Vector_executeFunc(Vector* restrict vector, uint32_t loc, void * (*func)(void*, void*), void* args) {
    if (vector->count == 0);
    else {
        if (loc >= vector->count) loc = (vector->size + vector->last - 1) % vector->size;
        else loc = (vector->first + loc) % vector->size;
        void** target = &vector->head[loc];
        if ((*target = func(*target, args)) == NULL) {
            register uint32_t actualTarget;
            register uint32_t csize = vector->size;
            register void** bufferHead = vector->head;
            register uint32_t count = vector->count;
            if (loc <= ((count - 1) >> 1)) {
                actualTarget = vector->first;
                vector->first = (actualTarget + 1) % csize;
                while (loc != actualTarget) {
                    register uint32_t next;
                    bufferHead[loc] = bufferHead[next = ((csize + loc) - 1) % csize];
                    loc = next;
                }
            }
            else {
                actualTarget = (csize + vector->last - 1) % csize;
                vector->last = actualTarget;
                while (loc != actualTarget) {
                    register uint32_t next;
                    bufferHead[loc] = bufferHead[next = (loc + 1) % csize];
                    loc = next;
                }
            }
            vector->count = count = count - 1;
            if (count <= (csize >> 1) && csize > 1 && count > 0) {
                uint32_t size;
                refactor(vector, (size = (csize * 3) >> 2, size <= 1) ? 1 : size);
            }
        }
    }
}

void Vector_swap(Vector* restrict vector, uint32_t pos1, uint32_t pos2) {
    if (vector->count >= 2) {
        if (pos1 >= vector->count) pos1 = (vector->size + vector->last - 1) % vector->size; 
        else pos1 = (vector->first + pos1) % vector->size;
        if (pos2 >= vector->count) pos2 = (vector->size + vector->last - 1) % vector->size;
        else pos2 = (vector->first + pos2) % vector->size;
        void* temp = vector->head[pos1];
        vector->head[pos1] = vector->head[pos2];
        vector->head[pos2] = temp;
    }
    return;
}

void Vector_clear(Vector* restrict vector, void* (*func)(void*, void*), void* args){
    uint32_t i = 0;
    while (i < vector->count) func(vector->head[(vector->first + i++) % vector->size],args);
    vector->size = 1;
    vector->count = 0;
    vector->first = vector->last = 0;
    free(vector->head);
    vector->head = (void**)malloc(sizeof(void**));
    return;
}
void Vector_destroy(Vector* restrict vector , void* (*func)(void*, void*), void* args){
    uint32_t i = 0;
    while (i < vector->count) func(vector->head[(vector->first + i++) % vector->size], args);
    free(vector->head);
    return;
}
void Vector_create(Vector * res ,uint32_t size){
    res->size = 1 + (size > 1) * (size - 1);
    res->head = (void**)malloc(res->size * sizeof(void*));
    res->count = 0;
    res->first = 0;
    res->last = 0;
}