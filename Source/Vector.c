#include "../Headers/vector.h"

static inline uint32_t uplimit(const uint32_t number,const uint32_t ceiling) {
    return number - (((number < ceiling) - 1) & ceiling);
}

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
                mainLoc[uplimit(counter , tempDest)] = mainLoc[i];
                counter += 1;
            }
            vector->last = uplimit(counter , tempDest);
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
    else if (loc >= vector->count) result = vector->head[uplimit(vector->size + vector->last - 1 , vector->size)];
    else result = vector->head[uplimit(vector->first + loc , vector->size)];
    return result;
}

void Vector_insert(Vector* restrict vector, uint32_t loc , createFunc creator , const void* obj) {
    register uint32_t cSize = vector->size;
    register uint32_t count = vector->count;
    if (count == cSize) refactor(vector, (cSize = cSize + (((cSize << 1) + cSize) >> 2)));
    register void** bufferHead = vector->head;
    loc -= ((loc <= count) - 1) & (loc - count);
    if (loc < ((count + 1) >> 1)) {
        register uint32_t cFirst;
        loc = uplimit( (vector->first = cFirst = uplimit(cSize + vector->first - 1 , cSize)) + loc , cSize);
        while (cFirst != loc) {
            register uint32_t next;
            bufferHead[cFirst] = bufferHead[next = uplimit(cFirst + 1 , cSize)];
            cFirst = next;
        }
    }
    else {
        register uint32_t cLast;
        vector->last = uplimit(((cLast = vector->last) + 1) , cSize);
        loc = uplimit(vector->first + loc , cSize);
        while (cLast != loc) {
            register uint32_t next;
            bufferHead[cLast] = bufferHead[next = uplimit(cSize + cLast - 1 , cSize)];
            cLast = next;
        }
    }
    bufferHead[loc] = creator(obj);
    vector->count = count + 1;
    return;
}

void Vector_executeFunc(Vector* restrict vector, uint32_t loc, void * (*func)(void*, void*), void* args) { 
    if (vector->count != 0){
        register uint32_t csize = vector->size;
        register uint32_t count = vector->count;
        register uint32_t cFirst = vector->first;
        register uint32_t cLast = vector->last;
        if (loc >= count) loc = uplimit(csize + cLast - 1 , csize);
        else loc = uplimit(cFirst + loc , csize);
        void** target = &vector->head[loc];
        void* newval = func(*target, args);
        if (newval == NULL) {
            register void** bufferHead = vector->head;
            if (((-(loc > cLast) & (loc - cLast)) | (-(loc <= cLast) & (cLast - loc))) > ((-(loc > cFirst) & (loc - cFirst)) | (-(loc <= cFirst) & (cFirst - loc)))) {
                vector->first = uplimit(cFirst + 1 , csize);
                while (loc != cFirst) {
                    register uint32_t next;
                    bufferHead[loc] = bufferHead[next = uplimit(csize + loc - 1 , csize)];
                    loc = next;
                }
            }
            else {
                vector->last = cLast = uplimit(csize + cLast - 1, csize);
                while (loc != cLast) {
                    register uint32_t next;
                    bufferHead[loc] = bufferHead[next = uplimit(loc + 1, csize)];
                    loc = next;
                }
            }
            vector->count = count = count - 1;
            if (count <= (csize >> 1) && csize > 4 && count > 0) {
                uint32_t size = ((csize << 1) + csize) >> 2;
                size = ( (-(size >= 4)) & size ) | ( (-(size < 4)) & 4 );
                refactor(vector, size);
            }
        }
        else *target = newval;
    }
}

void Vector_swap(Vector* restrict vector, uint32_t pos1, uint32_t pos2) {
    if (vector->count >= 2) {
        if (pos1 >= vector->count) pos1 = uplimit(vector->size + vector->last - 1 , vector->size);
        else pos1 = uplimit(vector->first + pos1 , vector->size);
        if (pos2 >= vector->count) pos2 = uplimit(vector->size + vector->last - 1, vector->size);
        else pos2 = uplimit(vector->first + pos2 , vector->size);
        void* temp = vector->head[pos1];
        vector->head[pos1] = vector->head[pos2];
        vector->head[pos2] = temp;
    }
    return;
}

void Vector_init_transfer(Vector* restrict dest, Vector* restrict src) {
    dest->head = src->head;
    dest->first = src->first;
    dest->last = src->last;
    dest->count = src->count;
    dest->size = src->size;
    src->head = NULL;
}

void Vector_clear(Vector* restrict vector, void* (*func)(void*, void*), void* args){
    uint32_t i = 0;
    while (i < vector->count) func(vector->head[uplimit(vector->first + i++, vector->size)],args);
    vector->size = 4;
    vector->count = 0;
    vector->first = vector->last = 0;
    free(vector->head);
    vector->head = (void**)malloc(4 * sizeof(void**));
    return;
}
void Vector_destroy(Vector* restrict vector , void* (*func)(void*, void*), void* args){
    uint32_t i = 0;
    while (i < vector->count) func(vector->head[uplimit(vector->first + i++ , vector->size)], args);
    free(vector->head);
    return;
}
void Vector_init(Vector * res){
    res->size = 4;
    res->head = (void**)malloc(4 * sizeof(void*));
    res->count = 0;
    res->first = 0;
    res->last = 0;
}