#include "../Headers/fragpath.h"
#include <stddef.h>
enum FragpathMode { Single = sizeof(int8_t), Double = sizeof(int16_t), Quadraple = sizeof(int32_t), DoubleQuadra = sizeof(int64_t) };

typedef struct jumpInstr_str { uint16_t jump; uint8_t size; } jumpInstruction;
typedef struct instruction {
    struct instruction* next;
#ifdef MultiThread
    mutex_var Mutex;
    cond_var CND_R;
    cond_var CND_W;
    cond_var CND_exp;
#endif
    uint16_t countofjumps;
    uint16_t workmode;
    jumpInstruction jumps[];
} instr;

static void compress(void* const result, const void* const addresses, const uint16_t countofjumps, const jumpInstruction* const jumps){
    for (uint16_t i = 0, currentoffset = 0; i < countofjumps; currentoffset += jumps[i++].size) {
        void* newAddress = (int8_t*)addresses + jumps[i].jump;
        void* newHome = (int8_t*)result + currentoffset;
        switch (jumps[i].size) {
            default:*(int8_t*)newHome = *(int8_t*)newAddress;break;
            case Double:*(int16_t*)newHome = *(int16_t*)newAddress;break;
            case Quadraple:*(int32_t*)newHome = *(int32_t*)newAddress;break;
            case DoubleQuadra:*(int64_t*)newHome = *(int64_t*)newAddress;
        }
    }
    return;
}

struct mthrd_str {
    Vector array;
#ifdef MultiThread
    uint8_t thrd_active_count;
    uint8_t writercount;
    uint8_t readercount;
#endif
};
struct generic_cell {
    uint8_t instrcount;
    uint8_t cell_type;
#ifdef MultiThread
    uint8_t cellProtector;
#endif
};

struct inner_cell {
    struct generic_cell parent;
    struct mthrd_str mthrd;
    int8_t area[];
};

struct leaf_cell {
    struct generic_cell parent;
    void* restrict context;
    int8_t area[];
};

#define seloffset(x) (((-(x)->cell_type) & offsetof(struct leaf_cell, area)) | (((x)->cell_type - 1) & offsetof(struct inner_cell, area)))

static instr* checkWhole(struct generic_cell* restrict cell, void* restrict data, const void* obj, instr* restrict start) {
    uint16_t offset = start->workmode;
    uint8_t limit = ((struct generic_cell *)cell)->instrcount;
    int8_t* dataOfCell = (uint8_t*)cell + seloffset(cell);
    while (start = start->next , --limit) {
        compress(data, obj, start->countofjumps, start->jumps);
        if (memcmp(dataOfCell + offset, data, start->workmode)) break;
        else offset += start->workmode;
    }
    return start;
}

static void searchVector(Vector* array, const void* dataOfAddress,instr * restrict element, struct generic_cell* restrict* restrict const place , uint32_t * restrict const flag) {
    uint32_t count = Vector_count(array);
    if (count) {
        int result;
        count -= 1;
        if (*place = Vector_get(array, (*flag = 0)),!(result = memcmp((char*)(*place) + seloffset(*place), dataOfAddress, element->workmode)));
        else if (!count || result > 0) {*flag = result < 0;*place = NULL;}
        else if (*place = Vector_get(array, (*flag = count)),!(result = memcmp((char*)(*place)+ seloffset(*place), dataOfAddress, element->workmode)));
        else if (result < 0) {*flag = count + 1;*place = NULL;}
        else {
            uint32_t offset = 0;
            while (1)
                if (count - offset <= 1) {
                    *flag = count;
                    *place = NULL;
                    break;
                }
                else if (*place = Vector_get(array, (*flag = (count + offset) >> 1)),!(result = memcmp((char*)(*place)+ seloffset(*place), dataOfAddress, element->workmode)))break;
                else if (result > 0) count = *flag;
                else offset = *flag;
        }
    }
    else {
        *flag = 0;
        *place = NULL;
    }
}

static instr* restrict Fragpath_iterator_READ(struct mthrd_str* path,
    instr* restrict element,
    void* const restrict dataOfAddress,
    void* restrict ** const restrict addressOut,
    const void* const address
#ifdef MultiThread
    , struct generic_cell* restrict* restrict const priorplace,
    instr* restrict* restrict const priorInstr
#endif
){
    uint8_t loopvar;
#ifdef MultiThread
    *priorplace = NULL;
    *priorInstr = NULL;
#endif
    do {
        struct generic_cell* restrict place;
        uint32_t flag;
        compress(dataOfAddress, address, element->countofjumps, element->jumps);
#ifdef MultiThread
        mutex_lock(&element->Mutex);
        path->readercount += 1;
        while(path->thrd_active_count & 1)cond_sleep(&element->CND_R, &element->Mutex);
        path->readercount -= 1;
        path->thrd_active_count += 2;
        mutex_unlock(&element->Mutex);
#endif
        searchVector(&path->array, dataOfAddress, element, &place, &flag);
#ifdef MultiThread
        mutex_lock(&element->Mutex);
        path->thrd_active_count -= 2;
        if(place)place->cellProtector += 2;
        if (!path->thrd_active_count && path->writercount)cond_broadcast(&element->CND_W);
        mutex_unlock(&element->Mutex);
        if (*priorInstr) {
            mutex_lock(&(*priorInstr)->Mutex);
            if (((*priorplace)->cellProtector -= 2) == 1)cond_broadcast(&(*priorInstr)->CND_exp);
            mutex_unlock(&(*priorInstr)->Mutex);
        }
#endif
        if (place != NULL) {
            instr* tempElement = element;
            instr* uncommonElement = checkWhole(place, dataOfAddress, address, element);
            uint8_t dist = 0;
            do dist += 1; while ((tempElement = tempElement->next) != uncommonElement);
            if (dist == ((struct generic_cell*)place)->instrcount) {
                if (place->cell_type == 0) {
                    path = &((struct inner_cell*)place)->mthrd;
                    loopvar = 1;
                }
                else {
                    *addressOut = &((struct leaf_cell*)place)->context;
                    loopvar = 0;
                }
            }
            else {
#ifdef MultiThread
                mutex_lock(&element->Mutex);
                if ((place->cellProtector -= 2) == 1)cond_broadcast(&element->CND_exp);
                mutex_unlock(&element->Mutex);
#endif
                loopvar = 0;
            }
#ifdef MultiThread
            *priorplace = place;
            *priorInstr = element;
#endif
            element = uncommonElement;
        }
        else loopvar = 0;
        
    } while (loopvar);
    return element;
}
static instr* restrict Fragpath_iterator_WRITE(
    struct mthrd_str* path,
    instr* restrict element,
    void* const restrict dataOfAddress,
    uint32_t* const restrict arithOut,
    struct generic_cell* restrict* const restrict priorplace,
    const void* const address,
    uint8_t* common_count
#ifdef MultiThread
    ,instr ** restrict const priorInstr
#endif
){
    uint8_t loopvar;
    *priorplace = NULL;
#ifdef MultiThread
    *priorInstr = NULL;
#endif
    do {
        struct generic_cell* restrict place;
        uint32_t flag;
        compress(dataOfAddress, address, element->countofjumps, element->jumps);
#ifdef MultiThread
        mutex_lock(&element->Mutex);
        path->writercount += 1;
        while(path->thrd_active_count)cond_sleep(&element->CND_W, &element->Mutex);
        path->writercount -= 1;
        path->thrd_active_count += 1;
        mutex_unlock(&element->Mutex);
#endif
        searchVector(&path->array, dataOfAddress, element, &place, &flag);
        if (place == NULL) {
            *arithOut = flag;
            *common_count = 0;
            loopvar = 0;
        }
        else {
            instr* tempElement = element;
            instr* uncommonElement = checkWhole(place, dataOfAddress, address, element);
            uint8_t dist = 0;
            do dist += 1; while ((tempElement = tempElement->next) != uncommonElement);
            if (dist != place->instrcount) {
                *common_count = dist;
                *arithOut = flag;
                loopvar = 0;
            }
            else {
#ifdef MultiThread
                mutex_lock(&element->Mutex);
                path->thrd_active_count -= 1;
                if (path->writercount)cond_broadcast(&element->CND_W);
                else if (path->readercount)cond_broadcast(&element->CND_R);
                place->cellProtector += 2;
                if (*priorInstr) {
                    mutex_lock(&(*priorInstr)->Mutex);
                    if (((*priorplace)->cellProtector -= 2) == 1)cond_broadcast(&(*priorInstr)->CND_exp);
                    mutex_unlock(&(*priorInstr)->Mutex);
                }
                mutex_unlock(&element->Mutex);
#endif
                if (place->cell_type == 0) {
                    path = &((struct inner_cell*)place)->mthrd;
                    loopvar = 1;
                }
                else loopvar = 0;
                *priorplace = place;
#ifdef MultiThread
                *priorInstr = element;
#endif
                element = uncommonElement;
            }
        }
    } while (loopvar);
    return element;
}

static instr* restrict Fragpath_iterator_ERASE(
    struct mthrd_str* restrict path,
    instr* restrict element,
    void* const restrict dataOfAddress,
    uint32_t* restrict arithOut,
    struct generic_cell* restrict * restrict priorplace,
    const void* const address,
    instr** restrict priorInstr
){
    uint8_t loopvar;
    struct generic_cell* restrict place, * restrict prevplace = NULL;
    struct mthrd_str* restrict realStruct = path;
    arithOut[2] = 0;
    priorInstr[0] = NULL;
    do {
        uint32_t flag;
        compress(dataOfAddress, address, element->countofjumps, element->jumps);
#ifdef MultiThread
        mutex_lock(&element->Mutex);
        path->writercount += 1;
        while(path->thrd_active_count)cond_sleep(&element->CND_W, &element->Mutex);
        path->writercount -= 1;
        path->thrd_active_count += 1;
        mutex_unlock(&element->Mutex);
#endif
        searchVector(&path->array, dataOfAddress, element, &place, &flag);
        if (place != NULL) {
            uint8_t dist = 0;
            instr* tempElement = element;
            instr* uncommonElement = checkWhole(place, dataOfAddress, address, element);
            do dist += 1; while ((tempElement = tempElement->next) != uncommonElement);
            if (place->instrcount == dist) {
                switch (arithOut[2]) {
                default:
#ifdef MultiThread
                    mutex_lock(&priorInstr[1]->Mutex);
                    realStruct->thrd_active_count -= 1;
                    if (realStruct->writercount)cond_broadcast(&priorInstr[1]->CND_W);
                    else if (realStruct->readercount)cond_broadcast(&priorInstr[1]->CND_R);
                    if (priorplace[1]) {
                        mutex_lock(&priorInstr[2]->Mutex);
                        if ((priorplace[1]->cellProtector -= 2) == 1)cond_broadcast(&priorInstr[2]->CND_exp);
                        mutex_unlock(&priorInstr[2]->Mutex);
                    }
                    mutex_unlock(&priorInstr[1]->Mutex);
                    priorInstr[2] = priorInstr[1];
#endif
                    realStruct = &((struct inner_cell*)priorplace[0])->mthrd;
                case 1:
                    arithOut[1] = arithOut[0];
                    priorInstr[1] = priorInstr[0];
                    priorplace[1] = priorplace[0];
                case 0:
                    arithOut[0] = flag;
                    priorInstr[0] = element;
                    priorplace[0] = prevplace;
                }
                arithOut[2] += 1;
                prevplace = place;
                if (place->cell_type == 0) {
#ifdef MultiThread
                    mutex_lock(&element->Mutex);
                    place->cellProtector += 2;
                    mutex_unlock(&element->Mutex);
#endif
                    path = &((struct inner_cell*)place)->mthrd;
                    loopvar = 1;
                }
                else loopvar = 0;
                element = uncommonElement;
                continue;
            }
        }
#ifdef MultiThread
        mutex_lock(&element->Mutex);
        path->thrd_active_count -= 1;
        if (path->writercount)cond_broadcast(&element->CND_W);
        else if (path->readercount)cond_broadcast(&element->CND_R);
        if (priorInstr[0]) {
            mutex_lock(&priorInstr[0]->Mutex);
            if ((prevplace->cellProtector -= 2) == 1)cond_broadcast(&priorInstr[0]->CND_exp);
            mutex_unlock(&priorInstr[0]->Mutex);
        }
        mutex_unlock(&element->Mutex);
    #endif
        loopvar = 0;
    } while (loopvar);
    return element;
}


struct exec_struct {
    union { void* (*func)(void*, void*); void (*destructor)(void*); };
    void* Args;
    instr* cinstr;
    char destroy;
};

static void* executeIn(void* cell, void* disposeVar) {
    instr* tempinstr = ((struct exec_struct*)disposeVar)->cinstr;
#ifdef MultiThread
    mutex_lock(&tempinstr->Mutex);
    ((struct generic_cell*)cell)->cellProtector += 1;
    while (((struct generic_cell*)cell)->cellProtector & (~1)) cond_sleep(&tempinstr->CND_exp, &tempinstr->Mutex);
    ((struct generic_cell*)cell)->cellProtector -= 1;
    mutex_unlock(&tempinstr->Mutex);
#endif
    if (((struct generic_cell*)cell)->cell_type == 0) {
        instr* newinstr = tempinstr;
        for (uint8_t i = 0; i < ((struct generic_cell*)cell)->instrcount; i++)newinstr = newinstr->next;
        ((struct exec_struct*)disposeVar)->cinstr = newinstr;
        for (uint32_t i = 0, count = Vector_count(&((((struct inner_cell*)cell)->mthrd).array)); i < count; i++)
            Vector_executeFunc(&((((struct inner_cell*)cell)->mthrd).array),i,executeIn, disposeVar);
        Vector_destroy(&((((struct inner_cell*)cell)->mthrd).array), NULL);
        ((struct exec_struct*)disposeVar)->cinstr = tempinstr;
        free(cell);
        cell = NULL;
    }
    else {
        if (((struct exec_struct*)disposeVar)->destroy || (((struct leaf_cell*)cell)->context = ((struct exec_struct*)disposeVar)->func(((struct leaf_cell*)cell)->context, ((struct exec_struct*)disposeVar)->Args)) == NULL) {
            if (((struct exec_struct*)disposeVar)->destroy && ((struct exec_struct*)disposeVar)->destructor) ((struct exec_struct*)disposeVar)->destructor(((struct leaf_cell*)cell)->context);
            free(cell);
            cell = NULL;
        }
    }
    return cell;
}

struct cell_args {
    const void* obj;
    void* restrict newObj;
    instr* cints;
    void* tempUn;
    uint8_t common_count;
};

static void* restrict makeLeaf(struct cell_args* args) {
    instr* tempInstr = args->cints;
    uint8_t depth = 1;
    void* result;
    {
        uint16_t size = args->cints->workmode;
        while ((tempInstr = tempInstr->next) != NULL) {
            size += tempInstr->workmode;
            depth += 1;
        }
        result = malloc(sizeof(struct leaf_cell) + size);
    }
    ((struct leaf_cell*)result)->parent.instrcount = depth;
    tempInstr = args->cints;
    memcpy(((struct leaf_cell*)result)->area, args->tempUn, tempInstr->workmode);
    uint16_t offset = tempInstr->workmode;
    tempInstr = tempInstr->next;
    for (uint8_t i = 1; i < depth; i++) {
        compress(args->tempUn, args->obj, tempInstr->countofjumps, tempInstr->jumps);
        memcpy(((struct leaf_cell*)result)->area + offset, args->tempUn, tempInstr->workmode);
        offset += tempInstr->workmode;
        tempInstr = tempInstr->next;
    }
    ((struct leaf_cell*)result)->context = args->newObj;
    ((struct leaf_cell*)result)->parent.cell_type = 1;
#ifdef MultiThread
    ((struct leaf_cell*)result)->parent.cellProtector = 0;
#endif
    return result;
}

static void * expandCell(void * cell, void * args ) {
#ifdef MultiThread
    mutex_lock(&((struct cell_args*)args)->cints->Mutex);
    ((struct generic_cell*)cell)->cellProtector += 1;
    while(((struct generic_cell*)cell)->cellProtector & (~1)) cond_sleep(&((struct cell_args*)args)->cints->CND_exp, &((struct cell_args*)args)->cints->Mutex);
    ((struct generic_cell*)cell)->cellProtector -= 1;
    mutex_unlock(&((struct cell_args*)args)->cints->Mutex);
#endif
    uint8_t common_count = ((struct cell_args*)args)->common_count;
    uint8_t type = ((struct generic_cell*)cell)->cell_type;
    uint16_t commonSize = 0;
    uint16_t uncommonSize = 0;
    uint8_t newDepth = ((struct generic_cell*)cell)->instrcount - common_count;
    uint8_t arreaOffset = ((-type) & offsetof(struct leaf_cell, area)) | ((type - 1) & offsetof(struct inner_cell, area));
    {
        instr* generalInst = ((struct cell_args*)args)->cints;
        for (uint8_t i = 0; i < common_count; i++) {
            commonSize += generalInst->workmode;
            generalInst = generalInst->next;
        }
        for (uint8_t i = 0; i < newDepth; i++) {
            uncommonSize += generalInst->workmode;
            generalInst = generalInst->next;
        }
    }
    struct generic_cell* restrict NewObj = malloc(((-type) & sizeof(struct leaf_cell) | ((type - 1) & sizeof(struct inner_cell))) + uncommonSize) ;
    NewObj->cell_type = type;
    NewObj->instrcount = newDepth;
#ifdef MultiThread
    NewObj->cellProtector = 0;
#endif
    memcpy((char *)NewObj + arreaOffset, (uint8_t*)cell + arreaOffset + commonSize, uncommonSize);
    if (type) ((struct leaf_cell*)NewObj)->context = ((struct leaf_cell*)cell)->context;
    else {
        Vector_init_transfer(&((struct inner_cell*)NewObj)->mthrd.array, &((struct inner_cell*)cell)->mthrd.array);
#ifdef MultiThread
        ((struct inner_cell*)NewObj)->mthrd.thrd_active_count = 0;
        ((struct inner_cell*)NewObj)->mthrd.readercount = 0;
        ((struct inner_cell*)NewObj)->mthrd.writercount = 0;
#endif
    }
    {
        void * newcell = malloc(sizeof(struct inner_cell) + commonSize);
        memcpy(((struct inner_cell*)newcell)->area, (char*)cell + arreaOffset, commonSize);
        free(cell);
        cell = newcell;
    }
    Vector_init(&((struct inner_cell*)cell)->mthrd.array);
    Vector_insert(&((struct inner_cell*)cell)->mthrd.array, 0, NewObj);
    ((struct inner_cell*)cell)->parent.instrcount = common_count;
    ((struct inner_cell*)cell)->parent.cell_type = 0;
#ifdef MultiThread
    ((struct inner_cell*)cell)->mthrd.thrd_active_count = 0;
    ((struct inner_cell*)cell)->mthrd.readercount = 0;
    ((struct inner_cell*)cell)->mthrd.writercount = 0;
    ((struct inner_cell*)cell)->parent.cellProtector = 0;
#endif
    do ((struct cell_args*)args)->cints = ((struct cell_args*)args)->cints->next; while (--common_count);
    Vector_insert(&((struct inner_cell*)cell)->mthrd.array, 1, makeLeaf(args));
    if (memcmp((int8_t *)Vector_get(&((struct inner_cell*)cell)->mthrd.array, 0) + arreaOffset, ((struct leaf_cell*)Vector_get(&((struct inner_cell*)cell)->mthrd.array, 1))->area, ((struct cell_args*)args)->cints->workmode) > 0)
        Vector_swap(&((struct inner_cell*)cell)->mthrd.array, 0, 1);
    return cell;
}

void Fragpath_insert(Fragpath* fragpath, const void* obj , void * restrict newObj){
    struct cell_args entry;
    struct generic_cell* restrict temp;
#ifdef MultiThread
    instr* priorInstr;
#endif
    uint32_t place;
    entry.obj = obj;
    entry.newObj = newObj;
    entry.tempUn = malloc(fragpath->largestByte);
#ifdef MultiThread
    instr * firstInstr = Fragpath_iterator_WRITE((struct mthrd_str *)&fragpath->array , fragpath->objStructSizes,entry.tempUn, &place, &temp, obj, &entry.common_count, &priorInstr);
#else
    instr* firstInstr = Fragpath_iterator_WRITE((struct mthrd_str*)&fragpath->array, fragpath->objStructSizes, entry.tempUn, &place, &temp, obj, &entry.common_count);
#endif
    entry.cints = firstInstr;
    if (firstInstr) {
        struct mthrd_str* realStruct = (temp != NULL) ? &((struct inner_cell*)temp)->mthrd : (struct mthrd_str*)&fragpath->array;
        if (entry.common_count) Vector_executeFunc(&realStruct->array, place, expandCell, &entry);
        else Vector_insert(&realStruct->array, place, makeLeaf(&entry));
#ifdef MultiThread
        mutex_lock(&firstInstr->Mutex);
        realStruct->thrd_active_count -= 1;
        if (realStruct->writercount)cond_broadcast(&firstInstr->CND_W);
        else if (realStruct->readercount)cond_broadcast(&firstInstr->CND_R);
        mutex_unlock(&firstInstr->Mutex);
#endif
    }
#ifdef MultiThread
    if(priorInstr){
        mutex_lock(&priorInstr->Mutex);
        if((((struct generic_cell*)temp)->cellProtector -= 2) == 1)cond_broadcast(&priorInstr->CND_exp);
        mutex_unlock(&priorInstr->Mutex);
    }
#endif
    free(entry.tempUn);
    return;
}
void* Fragpath_contains(Fragpath* fragpath, const void* obj){
    void* destination;
    void* data = malloc(fragpath->largestByte);
    void * restrict* temp;
#ifdef MultiThread
    instr* restrict priorInstr;
    struct generic_cell* restrict priorplace;
    instr* next = Fragpath_iterator_READ((struct mthrd_str*)&fragpath->array, fragpath->objStructSizes, data, &temp, obj, &priorplace, &priorInstr);
#else
    instr* next = Fragpath_iterator_READ((struct mthrd_str*)&fragpath->array, fragpath->objStructSizes, data, &temp, obj);
#endif
    if (next == NULL) {
        destination = *temp;
#ifdef MultiThread
        mutex_lock(&priorInstr->Mutex);
        if ((((struct generic_cell*)priorplace)->cellProtector -= 2) == 1)cond_broadcast(&priorInstr->CND_exp);
        mutex_unlock(&priorInstr->Mutex);
#endif
    }
    else destination = NULL;
    free(data);
    return destination;
}

static void* reduceCell(void* cell, void* cinstr) {
#ifdef MultiThread
    mutex_lock(&((instr**)cinstr)[0]->Mutex);
    ((struct inner_cell*)cell)->mthrd.thrd_active_count -= 1;
    if (((struct inner_cell*)cell)->mthrd.writercount)cond_broadcast(&((instr**)cinstr)[0]->CND_W);
    else if (((struct inner_cell*)cell)->mthrd.readercount)cond_broadcast(&((instr**)cinstr)[0]->CND_R);
    mutex_unlock(&((instr**)cinstr)[0]->Mutex);
    mutex_lock(&((instr**)cinstr)[1]->Mutex);
    ((struct generic_cell*)cell)->cellProtector -= 1;
    while (((struct generic_cell*)cell)->cellProtector & (~1)) cond_sleep(&((instr**)cinstr)[1]->CND_exp, &((instr**)cinstr)[1]->Mutex);
    ((struct generic_cell*)cell)->cellProtector -= 1;
    mutex_unlock(&((instr**)cinstr)[1]->Mutex);
#endif
    if (Vector_count(&((struct inner_cell*)cell)->mthrd.array) < 2) {
        struct generic_cell* restrict downcell = Vector_get(&((struct inner_cell*)cell)->mthrd.array, 0);
        Vector_destroy(&((struct inner_cell*)cell)->mthrd.array, NULL);
#ifdef MultiThread
        mutex_lock(&((instr**)cinstr)[0]->Mutex);
        downcell->cellProtector += 1;
        while (downcell->cellProtector & (~1)) cond_sleep(&((instr**)cinstr)[0]->CND_exp, &((instr**)cinstr)[0]->Mutex);
        downcell->cellProtector -= 1;
        mutex_unlock(&((instr**)cinstr)[0]->Mutex);
#endif
        uint8_t newcount = ((struct generic_cell*)cell)->instrcount + downcell->instrcount;
        uint16_t downsize = 0, upsize = 0;
        uint8_t i;
        instr* tempinstr = ((instr**)cinstr)[1];
        for (i = 0; i < ((struct generic_cell*)cell)->instrcount; i++) {
            upsize += tempinstr->workmode;
            tempinstr = tempinstr->next;
        }
        for (; i < newcount; i++) {
            downsize += tempinstr->workmode;
            tempinstr = tempinstr->next;
        }
        uint16_t totalSize = upsize + downsize;
        int8_t* buffer = malloc(totalSize);
        memcpy(buffer, ((struct inner_cell*)cell)->area, upsize);
        memcpy(buffer + upsize, (char*)downcell + seloffset(downcell), downsize);
        free(cell);
        cell = malloc((((downcell->cell_type - 1) & sizeof(struct inner_cell)) | ((-downcell->cell_type) & sizeof(struct leaf_cell))) + totalSize);
        if (downcell->cell_type == 0) {
            Vector_init_transfer(&((struct inner_cell*)cell)->mthrd.array, &((struct inner_cell*)downcell)->mthrd.array);
#ifdef MultiThread
            ((struct inner_cell*)cell)->mthrd.thrd_active_count = 0;
            ((struct inner_cell*)cell)->mthrd.readercount = 0;
            ((struct inner_cell*)cell)->mthrd.writercount = 0;
#endif
        }
        else ((struct leaf_cell*)cell)->context = ((struct leaf_cell*)downcell)->context;
        memcpy((char*)cell + seloffset(downcell), buffer, totalSize);
        ((struct generic_cell*)cell)->cell_type = downcell->cell_type;
        ((struct generic_cell*)cell)->instrcount = newcount;
#ifdef MultiThread
        ((struct generic_cell*)cell)->cellProtector = 0;
#endif
        free(downcell);
        free(buffer);
    }
    return cell;
}

static void Fragpath_modify(Fragpath* fragpath, const void* obj , struct exec_struct * disposal){
#ifdef MultiThread
    instr* priorInstr[3];
#else
    instr* priorInstr[2];
#endif
    struct inner_cell* restrict priorplace[2];
    uint32_t locs[3];
    struct mthrd_str* restrict realStruct[2] = {(struct mthrd_str * restrict)&fragpath->array , (struct mthrd_str* restrict) & fragpath->array};
    void* data = malloc(fragpath->largestByte);
    instr* next = Fragpath_iterator_ERASE((struct mthrd_str*)&fragpath->array, fragpath->objStructSizes,data, locs, (struct generic_cell * restrict * restrict)priorplace, obj, priorInstr);
    if (locs[2] >= 2) {
        realStruct[0] = &priorplace[0]->mthrd;
        if (locs[2] > 2)realStruct[1] = &priorplace[1]->mthrd;
    }
    disposal->cinstr = priorInstr[0];
#ifdef MultiThread
    uint8_t last;
    if (next == NULL) {
        Vector_executeFunc(&realStruct[0]->array, locs[0], executeIn, disposal);
        if (locs[2] >= 2) {
            Vector_executeFunc(&realStruct[1]->array, locs[1], reduceCell, priorInstr);
            last = 1;
        }
        else last = 0;
    }
    else last = 0;
    for (uint8_t i = ( (-(locs[2] <= 2)) & locs[2]) | ( (-(locs[2] > 2)) & 2); i > last; i--) {
        mutex_lock(&priorInstr[i - 1]->Mutex);
        realStruct[i - 1]->thrd_active_count -= 1;
        if (realStruct[i - 1]->writercount)cond_broadcast(&priorInstr[i - 1]->CND_W);
        else if (realStruct[i - 1]->readercount)cond_broadcast(&priorInstr[i - 1]->CND_R);
        if (priorplace[i - 1]) {
            mutex_lock(&priorInstr[i]->Mutex);
            if ((priorplace[i - 1]->parent.cellProtector -= 2) == 1)cond_broadcast(&priorInstr[i]->CND_exp);
            mutex_unlock(&priorInstr[i]->Mutex);
        }
        mutex_unlock(&priorInstr[i - 1]->Mutex);
    }
#else
    if (next == NULL) {
        Vector_executeFunc(&realStruct[0]->array, locs[0], executeIn, disposal);
        if (locs[2] >= 2) Vector_executeFunc(&realStruct[1]->array, locs[1], reduceCell, priorInstr);
    }
#endif
    free(data);
    return;
}

void Fragpath_executeFunc(Fragpath* fragpath, const void* obj, void* (*func)(void*, void*), void* args) {
    struct exec_struct disposal = { .func = func , .Args = args ,.destroy = 0 };
    Fragpath_modify(fragpath, obj, &disposal);
}

void Fragpath_delete(Fragpath* fragpath, const void* obj, void (*func)(void*)) {
    struct exec_struct disposal = { .destructor = func ,.destroy = 1 };
    Fragpath_modify(fragpath, obj, &disposal);
}

struct common {
    LinkedList list;
    uint16_t bytes;
    void* obj;
};

static void getKeys_Rec(instr* restrict instructions, struct mthrd_str * array, struct common* runcfg){
#ifdef MultiThread
    mutex_lock(&instructions->Mutex);
    array->readercount += 1;
    while(array->thrd_active_count & 1)cond_sleep(&instructions->CND_R, &instructions->Mutex);
    array->readercount -= 1;
    array->thrd_active_count += 2;
    mutex_unlock(&instructions->Mutex);
#endif
    for (uint32_t i = 0, count = Vector_count(&array->array); i < count; i++) {
        struct generic_cell* cell = Vector_get(&array->array, i);
        uint16_t size = instructions->countofjumps;
        {
            instr* tempInst = instructions->next;
            for (uint8_t i = cell->instrcount; i > 1; i--) {
                size += tempInst->countofjumps;
                tempInst = tempInst->next;
            }
        }
        uint32_t offset = 0;
        int8_t* newSource = (char *)cell + seloffset(cell);
        instr* currentInstr = instructions;
        uint16_t setback = 0;
        for (uint16_t k = 0; k < size; k++) {
            if ((k - setback) == currentInstr->countofjumps) {
                setback += currentInstr->countofjumps;
                currentInstr = currentInstr->next;
            }
            void* newDest = (int8_t*)runcfg->obj + currentInstr->jumps[k - setback].jump;
            void* finalSource = newSource + offset;
            switch (currentInstr->jumps[k - setback].size) {
                default:*((int8_t*)newDest) = *((int8_t*)finalSource);break;
                case Double:*((int16_t*)newDest) = *((int16_t*)finalSource);break;
                case Quadraple:*((int32_t*)newDest) = *((int32_t*)finalSource);break;
                case DoubleQuadra:*((int64_t*)newDest) = *((int64_t*)finalSource);
            }
            offset += currentInstr->jumps[k - setback].size;
        }
        if (cell->cell_type) LinkedList_insert(&runcfg->list, -1,DS_cpy(runcfg->bytes,runcfg->obj));
        else getKeys_Rec(currentInstr->next, &((struct inner_cell*)cell)->mthrd, runcfg);
    }
#ifdef MultiThread
    mutex_lock(&instructions->Mutex);
    array->thrd_active_count -= 2;
    if (!array->thrd_active_count && array->writercount)cond_broadcast(&instructions->CND_W);
    mutex_unlock(&instructions->Mutex);
#endif
}
void* Fragpath_getKeys(Fragpath* fragpath, uint32_t* count){
    struct common cfg;
    cfg.bytes = fragpath->bytes;
    cfg.obj = malloc(cfg.bytes);
    LinkedList_init(&cfg.list);
    getKeys_Rec(fragpath->objStructSizes, (struct mthrd_str*)&fragpath->array, &cfg);
    free(cfg.obj);
    void* result = malloc(fragpath->bytes * (*count = LinkedList_count(&cfg.list)));
    void* parser = result;
    while (LinkedList_count(&cfg.list) > 0) {
        memcpy(parser, LinkedList_get(&cfg.list, 0), fragpath->bytes);
        parser = (int8_t*)parser + fragpath->bytes;
        LinkedList_delete(&cfg.list,0, free);
    }
    return result;
}
void Fragpath_swap(Fragpath* fragpath, const void* key1, const void* key2){
    void* restrict* path1 ,* restrict* path2;
    void* data = malloc(fragpath->largestByte);
#ifdef MultiThread
    struct generic_cell* restrict place1, * restrict place2;
    instr* restrict previnstr1, * restrict previnstr2;
#endif
#ifdef MultiThread
    if (Fragpath_iterator_READ((struct mthrd_str *)&fragpath->array, fragpath->objStructSizes, data, &path1, key1, &place1, &previnstr1) == NULL) {
        if (Fragpath_iterator_READ((struct mthrd_str*)&fragpath->array, fragpath->objStructSizes, data, &path2, key2, &place2, &previnstr2) == NULL) {
#else
    if (Fragpath_iterator_READ((struct mthrd_str*)&fragpath->array, fragpath->objStructSizes, data, &path1, key1) == NULL) {
        if (Fragpath_iterator_READ((struct mthrd_str*)&fragpath->array, fragpath->objStructSizes, data, &path2, key2) == NULL) {
#endif
            void* temp = *path1;
            *path1 = *path2;
            *path2 = temp;
#ifdef MultiThread
            mutex_lock(&previnstr2->Mutex);
            if ((place2->cellProtector -= 2) == 1)cond_broadcast(&previnstr2->CND_exp);
            mutex_unlock(&previnstr2->Mutex);
        }
        mutex_lock(&previnstr1->Mutex);
        if ((place1->cellProtector -= 2) == 1)cond_broadcast(&previnstr1->CND_exp);
        mutex_unlock(&previnstr1->Mutex);
#else
        }
#endif
    }
    free(data);
    return;
}

typedef struct semi_instr_str {
    uint16_t workmode;
    uint16_t countofjumps;
    jumpInstruction* jumps;
}semi_instr;
struct pure_instr {
    uint16_t countofjumps;
    uint16_t workmode;
    jumpInstruction jumps[];
};

static void* restrict setInstr(semi_instr* data){
    uint16_t count = data->countofjumps, i = 0;
    struct pure_instr* result = (struct pure_instr*)malloc(sizeof(struct pure_instr) + (count * sizeof(jumpInstruction)));
    jumpInstruction* dest = result->jumps, * src = data->jumps;
    while (i < count) {
        dest[i].jump = src[i].jump;
        dest[i].size = src[i].size;
        i += 1;
    }
    result->countofjumps = count;
    result->workmode = data->workmode;
    return result;
}
static instr* Fragpath_init(LinkedList* const actualbytes, uint16_t* const restrict largeOut){
    LinkedList steplist; 
    LinkedList_init(&steplist);
    instr* result;
    *largeOut = 0;
    {
        struct jumpcmd { uint8_t jumpSize; uint16_t segment; };
        uint8_t segcount = LinkedList_count(actualbytes);
        uint16_t bytes = 0, *bytesPerSegm = (uint16_t*)malloc(segcount * sizeof(uint16_t));
        for (uint8_t i = 0; i < segcount; ++i) {
            seginfo* restrict context = (seginfo*)LinkedList_get(actualbytes, i);
            bytes += (bytesPerSegm[i] = (*context)[1]);
        }
        LinkedList command;
        struct jumpcmd currJumpOp;
        LinkedList_init(&command);
        for (uint8_t i = 0 , temp = 1; i < segcount; ++i) while (bytesPerSegm[i]) {
            uint16_t maxStep = bytes - (-(temp < bytes) & (bytes - temp));
            bytes -= maxStep;
            temp = temp << 1;
            *largeOut += -(*largeOut < maxStep) & (maxStep - *largeOut);
            {
                uint8_t currentJump;
                {
                    uint16_t totalSize = 0, indexOfSegment = 0, sizeofsegm = bytesPerSegm[i + (indexOfSegment++)];
                    do if (sizeofsegm != 0) {
                        uint8_t tempVar = 8;
                        while (tempVar > sizeofsegm) tempVar = tempVar >> 1;
                        currentJump = tempVar;
                        currJumpOp.jumpSize = currentJump;
                        currJumpOp.segment = i + indexOfSegment - 1;
                        LinkedList_insert(&command,-1, DS_cpy(sizeof(struct jumpcmd), &currJumpOp));
                        totalSize += currentJump;
                        sizeofsegm -= currentJump;
                    }
                    else sizeofsegm = bytesPerSegm[i + (indexOfSegment++)];
                    while (totalSize < maxStep);
                    currentJump -= (totalSize - maxStep);
                }
                {
                    struct jumpcmd* temploc = LinkedList_get(&command,-1);
                    if (currentJump > Single && (currentJump & 1) != 0) {
                        currentJump -= 1;
                        currJumpOp.jumpSize = Single;
                        currJumpOp.segment = temploc->segment;
                        LinkedList_insert(&command,-1, DS_cpy(sizeof(struct jumpcmd), &currJumpOp));
                    }
                    if (currentJump == 6) {
                        currentJump -= 2;
                        currJumpOp.jumpSize = Double;
                        currJumpOp.segment = temploc->segment;
                        LinkedList_insert(&command,-1, DS_cpy(sizeof(struct jumpcmd), &currJumpOp));
                    }
                    temploc->jumpSize = currentJump;
                }
            }
            {
                uint16_t countofjumps = LinkedList_count(&command);
                semi_instr data = {
                    .workmode = maxStep,
                    .countofjumps = countofjumps,
                    .jumps = (jumpInstruction*)malloc(sizeof(jumpInstruction) * countofjumps)
                };
                uint16_t currentSegmentNum = ((struct jumpcmd*)LinkedList_get(&command, 0))->segment;
                seginfo* segment = (seginfo*)LinkedList_get(actualbytes, currentSegmentNum);
                for (uint16_t k = 0; k < countofjumps; ++k) {
                    struct jumpcmd* currentCommand = LinkedList_get(&command, k);
                    if (currentSegmentNum != currentCommand->segment) {
                        currentSegmentNum = currentCommand->segment;
                        segment = (seginfo*)LinkedList_get(actualbytes, currentSegmentNum);
                    }
                    data.jumps[k].size = currentCommand->jumpSize;
                    data.jumps[k].jump = ((*segment)[1] - bytesPerSegm[currentCommand->segment]) + (*segment)[0];
                    bytesPerSegm[currentCommand->segment] -= currentCommand->jumpSize;
                }
                LinkedList_insert(&steplist,-1 , setInstr(&data));
                free(data.jumps);
            }
            LinkedList_clear(&command, free);
        }
        free(bytesPerSegm);
        LinkedList_destroy(actualbytes,free);
        LinkedList_destroy(&command,free);
    }
    {
        uint16_t count = LinkedList_count(&steplist) - 1;
        struct pure_instr* reference = LinkedList_get(&steplist, count);
        while (count > 1) {
            struct pure_instr* new_reference = LinkedList_get(&steplist, count - 1);
            if (reference->workmode < new_reference->workmode) LinkedList_swap(&steplist, count, count - 1);
            else break;
            count -= 1;
        }
    }
    {
        instr** restrict dest = &result;
        do {
            uint16_t k, count;
            jumpInstruction* placeJumps, * workmodeJumps;
            struct pure_instr* workmode = (struct pure_instr*)LinkedList_get(&steplist, 0);
            (*dest)= (instr*)malloc(sizeof(instr) + (sizeof(jumpInstruction) * workmode->countofjumps));
            (*dest)->countofjumps = workmode->countofjumps;
            (*dest)->workmode = workmode->workmode;
            for (k = 0, count = (*dest)->countofjumps, placeJumps = (*dest)->jumps, workmodeJumps = workmode->jumps; k < count; k++) {
                placeJumps[k].jump = workmodeJumps[k].jump;
                placeJumps[k].size = workmodeJumps[k].size;
            }
#ifdef MultiThread
            mutex_init(&(*dest)->Mutex);
            cond_init(&(*dest)->CND_R);
            cond_init(&(*dest)->CND_W);
            cond_init(&(*dest)->CND_exp);
#endif
            LinkedList_delete(&steplist,0, free);
            dest = &(*dest)->next;
        } while (LinkedList_count(&steplist) > 0);
        (*dest) = NULL;
    }
    return result;
}
void Fragpath_create(Fragpath* fragpath,const seginfo* restrict objectStruct,uint8_t arrayCount){
    uint16_t objBytes;
    {
        LinkedList map; LinkedList_init(&map);
        uint8_t f = 0;
        {
            seginfo newseg , *currentSeg;
            currentSeg = NULL;
            newseg[1] = objectStruct[f][1];
            do {
                newseg[0] = objectStruct[f][0];
                if (currentSeg == NULL || newseg[0] != (*currentSeg)[0] + (*currentSeg)[1]) {
                    LinkedList_insert(&map, -1, DS_cpy(sizeof(seginfo), &newseg));
                    currentSeg = (seginfo*)LinkedList_get(&map, -1);
                }
                else (*currentSeg)[1] += newseg[1];
                f += 1;
            } while ((newseg[1] = objectStruct[f][1]) != 0);
        }
        objBytes = objectStruct[f][0];
        f = LinkedList_count(&map);
        {
            seginfo finalseg ,lastitem, *last , *currentTail;
            currentTail = (seginfo*)LinkedList_get(&map, f);
            lastitem[0] = (*currentTail)[0];
            lastitem[1] = (*currentTail)[1];
            for (uint8_t i = 1; i < arrayCount; ++i) {
                for (uint8_t k = 0; k < f; k++) {
                    if (f - k > 1)last = (seginfo*)LinkedList_get(&map, k);
                    else last = &lastitem;
                    finalseg[0] = (*last)[0] + (i * objBytes);
                    finalseg[1] = (*last)[1];
                    if (finalseg[0] == (*currentTail)[0] + (*currentTail)[1])(*currentTail)[1] += finalseg[1];
                    else {
                        LinkedList_insert(&map, -1, DS_cpy(sizeof(seginfo),&finalseg));
                        currentTail = (seginfo*)LinkedList_get(&map, -1);
                    }
                }
            }
        }
        fragpath->objStructSizes = Fragpath_init(&map, &fragpath->largestByte);
        Vector_init(&fragpath->array);
    }
#ifdef MultiThread
    fragpath->thrd_active_count = 0;
    fragpath->writercount = 0;
    fragpath->readercount = 0;
#endif
    fragpath->bytes = arrayCount * objBytes;
    return;
}
void Fragpath_destroy(Fragpath* fragpath, void (*func)(void*)){
    struct exec_struct disposal;
    disposal.destructor = func;
    disposal.cinstr = fragpath->objStructSizes;
    disposal.destroy = 1;
    for (uint32_t i = 0, count = Vector_count(&fragpath->array); i < count; i++)
        Vector_executeFunc(&fragpath->array, i, executeIn, &disposal);
    Vector_destroy(&fragpath->array, NULL);
    for (instr* place = fragpath->objStructSizes, *nextPlace; place != NULL; place = nextPlace) {
        nextPlace = place->next;
#ifdef MultiThread
        mutex_destroy(&place->Mutex);
        cond_destroy(&place->CND_R);
        cond_destroy(&place->CND_W);
        cond_destroy(&place->CND_exp);
#endif
        free(place);
    }
    return;
}
void Fragpath_clear(Fragpath* fragpath, void (*func)(void*)){
    struct exec_struct disposal;
    disposal.destructor = func;
    disposal.cinstr = fragpath->objStructSizes;
    disposal.destroy = 1;
    disposal.cinstr = fragpath->objStructSizes;
    for (uint32_t i = 0, count = Vector_count(&fragpath->array); i < count; i++)
        Vector_executeFunc(&fragpath->array, i, executeIn, &disposal);
    Vector_clear(&fragpath->array, NULL);
    return;
}