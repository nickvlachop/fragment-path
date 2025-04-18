#include "../Headers/DataStruct.h"

void* DS_nullify(void* cell, void* args) {
    if (args != NULL) ((void (*)(void*))args)(cell);
    return NULL;
}

void* DS_cpy(const void * args) {
    if (((struct DS_cpy_args*)args)->bytes == 0)return ((struct DS_cpy_args*)args)->obj;
    else if (((struct DS_cpy_args*)args)->obj == NULL) return malloc(((struct DS_cpy_args*)args)->bytes);
    else return memcpy(malloc(((struct DS_cpy_args *)args)->bytes), ((struct DS_cpy_args*)args)->obj, ((struct DS_cpy_args*)args)->bytes);
}