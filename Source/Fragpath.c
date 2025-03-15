#include "../Headers/fragpath.h"
#define cell_instrcount(cell) *(uint8_t*)cell
enum FragpathMode { Single = sizeof(int8_t), Double = sizeof(int16_t), Quadraple = sizeof(int32_t), DoubleQuadra = sizeof(int64_t) };

typedef struct jumpInstr_str { uint16_t jump; uint8_t size; } jumpInstruction;
typedef struct instruction {
    struct instruction* next;
#ifdef MultiThread
    mutex_var Mutex;
    cond_var CNDR;
    cond_var CNDW;
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
struct leaf {
    void* context;
};

struct mthrd_str {
    Vector array;
#ifdef MultiThread
    uint8_t thrdVar;
#endif
};

struct inner_cell {
    uint8_t instrcount;
    struct mthrd_str mthrd;
    int8_t area[];
};

struct leaf_cell {
    uint8_t instrcount;
    struct leaf leaf;
    int8_t area[];
};

static inline instr* checkWhole(void* restrict cell, void* restrict data, const void* obj, instr* restrict start) {
    uint16_t offset = start->workmode;
    while((start = start->next) != NULL) {
        compress(data, obj, start->countofjumps, start->jumps);
        if (memcmp(((struct leaf_cell*)cell)->area + offset, data, start->workmode)) break;
        else offset += start->workmode;
    }
    return start;
}

static void searchVector(Vector* array, const void* dataOfAddress,instr * restrict element,void ** restrict const place , uint32_t * restrict const flag) {
    uint32_t count = Vector_count(array);
    if (count) {
        int result;
        count -= 1;
        if (*place = Vector_get(array, (*flag = 0)) , 
            !(result = memcmp((char*)(*place)+ (cell_instrcount(*place) ? (offsetof(struct leaf_cell, area)) : offsetof(struct inner_cell, area) ), dataOfAddress, element->workmode)));
        else if (!count || result > 0) {
            *flag = result < 0;
            *place = NULL;
        }
        else if (*place = Vector_get(array, (*flag = count)),
            !(result = memcmp((char*)(*place)+ (cell_instrcount(*place) ? (offsetof(struct leaf_cell, area)) : offsetof(struct inner_cell, area)), dataOfAddress, element->workmode)));
        else if (result < 0) {
            *flag = count + 1;
            *place = NULL;
        }
        else {
            uint32_t offset = 0;
            while (1)
                if (count - offset <= 1) {
                    *flag = count;
                    *place = NULL;
                    break;
                }
                else if (*place = Vector_get(array, (*flag = (count + offset) >> 1)),
                    !(result = memcmp((char*)(*place)+ (cell_instrcount(*place) ? (offsetof(struct leaf_cell, area)) : offsetof(struct inner_cell, area)), dataOfAddress, element->workmode)))break;
                else if (result > 0) count = *flag;
                else offset = *flag;
        }
    }
    else {
        *flag = 0;
        *place = NULL;
    }
}

static instr* restrict Fragpath_iterator_READ(struct mthrd_str* path, instr* restrict element, void* const restrict dataOfAddress, void** const restrict addressOut, const void* const address) {
    uint8_t loopvar;
    do {
        void* place;
        uint32_t flag;
        compress(dataOfAddress, address, element->countofjumps, element->jumps);
#ifdef MultiThread
        mutex_lock(&element->Mutex);
        while (path->thrdVar & 1)cond_sleep(&element->CNDW, &element->Mutex);
        path->thrdVar += 2;
        mutex_unlock(&element->Mutex);
#endif
        searchVector(&path->array, dataOfAddress, element, &place, &flag);
        instr* restrict oldElement = element;
        struct mthrd_str* oldPath = path;
        if (place != NULL) {
            if (cell_instrcount(place) != 0) {
                instr* uncommonElement = checkWhole(place, dataOfAddress, address, element);
                *addressOut = &((struct leaf_cell*)place)->leaf;
                element = uncommonElement;
                loopvar = 0;
            }
            else {
                element = element->next;
                path = &((struct inner_cell*)place)->mthrd;
                loopvar = 1;
            }
        }
        else loopvar = 0;
#ifdef MultiThread
        mutex_lock(&oldElement->Mutex);
        oldPath->thrdVar -= 2;
        if (!(oldPath->thrdVar >> 1)) cond_broadcast(&oldElement->CNDR);
        mutex_unlock(&oldElement->Mutex);
#endif
    } while (loopvar);
    return element;
}
static instr* restrict Fragpath_iterator_WRITE(struct mthrd_str* path, instr* restrict element, void* const restrict dataOfAddress, uint32_t* const restrict arithOut, void** const restrict addressOut, const void* const address, uint8_t* common_count) {
    uint8_t loopvar;
    do {
        void* place;
        uint32_t flag;
        compress(dataOfAddress, address, element->countofjumps, element->jumps);
#ifdef MultiThread
        mutex_lock(&element->Mutex);
        while (1)
            if (path->thrdVar >> 1)cond_sleep(&element->CNDR, &element->Mutex);
            else if (path->thrdVar & 1)cond_sleep(&element->CNDW, &element->Mutex);
            else break;
        path->thrdVar += 1;
        mutex_unlock(&element->Mutex);
#endif
        searchVector(&path->array, dataOfAddress, element, &place, &flag);
        if (place == NULL) {
            *arithOut = flag;
            *addressOut = path;
            *common_count = 0;
            loopvar = 0;
        }
        else {
            instr* uncommonElement;
            if (cell_instrcount(place) != 0 && (uncommonElement = checkWhole(place, dataOfAddress, address, element)) != NULL) {
                *common_count = 0;
                instr* tempElement = element;
                do *common_count += 1; while ((tempElement = tempElement->next) != uncommonElement);
                *addressOut = path;
                *arithOut = flag;
                loopvar = 0;
            }
            else {
#ifdef MultiThread
                mutex_lock(&element->Mutex);
                path->thrdVar -= 1;
                cond_broadcast(&element->CNDW);
                mutex_unlock(&element->Mutex);
#endif
                if (cell_instrcount(place) == 0) {
                    element = element->next;
                    path = &((struct inner_cell*)place)->mthrd;
                    loopvar = 1;
                }
                else {
                    element = NULL;
                    loopvar = 0;
                }
            }
        }
    } while (loopvar);
    return element;
}

static instr* restrict Fragpath_iterator_ERASE(struct mthrd_str* path, instr* restrict element, void* const restrict dataOfAddress, uint32_t* const restrict arithOut, void** const restrict addressOut, const void* const address) {
    uint8_t erase_state = 0;
    uint8_t loopvar;
    do {
        void* place;
        uint32_t flag;
        if (erase_state) {
#ifdef MultiThread
            mutex_lock(&element->Mutex);
            while (1)
                if (path->thrdVar >> 1)cond_sleep(&element->CNDR, &element->Mutex);
                else if (path->thrdVar & 1)cond_sleep(&element->CNDW, &element->Mutex);
                else break;
            if (Vector_count(&path->array) > 1) {
                mutex_unlock(&element->Mutex);
                break;
            }
            else {
                path->thrdVar += 1;
                mutex_unlock(&element->Mutex);
                compress(dataOfAddress, address, element->countofjumps, element->jumps);
            }
#else
            if (Vector_count(&path->array) > 1)break;
            else compress(dataOfAddress, address, element->countofjumps, element->jumps);
#endif
        }
        else {
            compress(dataOfAddress, address, element->countofjumps, element->jumps);
#ifdef MultiThread
            mutex_lock(&element->Mutex);
            while (1)
                if (path->thrdVar >> 1)cond_sleep(&element->CNDR, &element->Mutex);
                else if (path->thrdVar & 1)cond_sleep(&element->CNDW, &element->Mutex);
                else break;
            path->thrdVar += 1;
            mutex_unlock(&element->Mutex);
#endif
        }
        searchVector(&path->array, dataOfAddress, element, &place, &flag);
        if (place == NULL || (cell_instrcount(place) != 0 && checkWhole(place, dataOfAddress, address, element) != NULL)) {
#ifdef MultiThread
            mutex_lock(&element->Mutex);
            path->thrdVar -= 1;
            cond_broadcast(&element->CNDW);
            mutex_unlock(&element->Mutex);
#endif
            * (addressOut + 1) = NULL;
            loopvar = 0;
        }
        else {
            if (erase_state == 0) {
                *arithOut = flag;
                *addressOut = path;
                erase_state = 1;
            }
#ifdef MultiThread
            else {
                mutex_lock(&element->Mutex);
                path->thrdVar -= 1;
                cond_broadcast(&element->CNDW);
                mutex_unlock(&element->Mutex);
            }
#endif
            if (cell_instrcount(place) == 0) {
                *(addressOut + 1) = &((struct inner_cell*)place)->mthrd.array;
                element = element->next;
                path = &((struct inner_cell*)place)->mthrd;
                loopvar = 1;
            }
            else {
                element = NULL;
                loopvar = 0;
            }
        }
    } while (loopvar);
    return element;
}


struct disposeCond {
    void* (*freeFunc)(void*, void*);
    void* freeArgs;
    char clearAll;
};

static void* executeIn(void* cell, void* disposeVar) {
    if (cell_instrcount(cell) == 0) {
        if (((struct disposeCond*)disposeVar)->clearAll ||
            (Vector_executeFunc(&((((struct inner_cell*)cell)->mthrd).array), 0, executeIn, disposeVar),  Vector_count(&((((struct inner_cell*)cell)->mthrd).array)) == 0)) {
            Vector_destroy(&((((struct inner_cell*)cell)->mthrd).array), executeIn, disposeVar);
            free(cell);
            cell = NULL;
        }
    }
    else if (((((struct leaf_cell*)cell)->leaf).context = ((struct disposeCond*)disposeVar)->freeFunc((((struct leaf_cell*)cell)->leaf).context, ((struct disposeCond*)disposeVar)->freeArgs)) == NULL){
        free(cell);
        cell = NULL;
    }
    return cell;
}

struct cell_args {
    const void* obj;
    const void* newObj;
    instr* cints;
    void* tempUn;
    createFunc type;
    uint8_t common_count;
};

struct inner_args {
    instr* cints;
    void* common;
};

struct leafEx_args {
    uint16_t size;
    void* uncommon;
    void* obj;
    uint8_t newDepth;
};

static void* restrict makeInner(const void* args) {
    uint16_t size = ((struct inner_args*)args)->cints->workmode;
    void* result = malloc(sizeof(struct inner_cell) + size);
    memcpy(((struct inner_cell*)result)->area, ((struct inner_args*)args)->common, size);
    Vector_create(&((struct inner_cell*)result)->mthrd.array, 2);
    ((struct inner_cell*)result)->instrcount = 0;
#ifdef MultiThread
    ((struct inner_cell*)result)->mthrd.thrdVar = 0;
#endif
    return result;
}

static void* restrict makeLeaf(const void* args) {
    instr* tempInstr = ((struct cell_args*)args)->cints;
    uint8_t depth = 1;
    void* result;
    {
        uint16_t size = ((struct cell_args*)args)->cints->workmode;
        while ((tempInstr = tempInstr->next) != NULL) {
            size += tempInstr->workmode;
            depth += 1;
        }
        result = malloc(sizeof(struct leaf_cell) + size);
    }
    ((struct leaf_cell*)result)->instrcount = depth;
    tempInstr = ((struct cell_args*)args)->cints;
    memcpy(((struct leaf_cell*)result)->area, ((struct cell_args*)args)->tempUn, tempInstr->workmode);
    uint16_t offset = tempInstr->workmode;
    tempInstr = tempInstr->next;
    for (uint8_t i = 1; i < depth; i++) {
        compress(((struct cell_args*)args)->tempUn, ((struct cell_args*)args)->obj, tempInstr->countofjumps, tempInstr->jumps);
        memcpy(((struct leaf_cell*)result)->area + offset, ((struct cell_args*)args)->tempUn, tempInstr->workmode);
        offset += tempInstr->workmode;
        tempInstr = tempInstr->next;
    }
    ((struct leaf_cell*)result)->leaf.context = ((struct cell_args*)args)->type(((struct cell_args*)args)->newObj);
    return result;
}

static void* restrict makeLeafEx(const void* args) {
    void* result = malloc(sizeof(struct leaf_cell) + ((struct leafEx_args *)args)->size);
    memcpy(((struct leaf_cell*)result)->area, ((struct leafEx_args*)args)->uncommon, ((struct leafEx_args*)args)->size);
    ((struct leaf_cell*)result)->leaf.context = ((struct leafEx_args*)args)->obj;
    ((struct leaf_cell*)result)->instrcount = ((struct leafEx_args*)args)->newDepth;
    return result;
}

static void * expandCell(void * cell, void * args ) {
    struct leafEx_args newLeaf;
    uint8_t common_count = ((struct cell_args*)args)->common_count;
    int8_t* common;
    {
        uint16_t commonSize = 0;
        instr* generalInst = ((struct cell_args*)args)->cints;
        for (uint8_t i = 0; i < common_count; i++) {
            commonSize += generalInst->workmode;
            generalInst = generalInst->next;
        }
        newLeaf.newDepth = ((struct leaf_cell*)cell)->instrcount - common_count;
        newLeaf.size = 0;
        for (uint8_t i = 0; i < newLeaf.newDepth; i++) {
            newLeaf.size += generalInst->workmode;
            generalInst = generalInst->next;
        }
        common = memcpy(malloc(commonSize), ((struct leaf_cell*)cell)->area, commonSize);
        newLeaf.uncommon = memcpy(malloc(newLeaf.size), ((struct leaf_cell*)cell)->area + commonSize, newLeaf.size);
    }
    newLeaf.obj = ((struct leaf_cell*)cell)->leaf.context;
    free(cell);
    uint16_t commonIndex = ((struct cell_args*)args)->cints->workmode;
    cell = malloc(sizeof(struct inner_cell) + commonIndex);
    ((struct inner_cell*)cell)->instrcount = 0;
    memcpy(((struct inner_cell*)cell)->area, common, commonIndex);
    Vector_create(&((struct inner_cell*)cell)->mthrd.array, 2);
#ifdef MultiThread
    ((struct inner_cell*)cell)->mthrd.thrdVar = 0;
#endif
    {
        void* cellBelow = cell;
        struct inner_args innerEntry = { .cints = ((struct cell_args*)args)->cints->next };
        for (uint8_t i = 1; i < common_count; i++) {
            innerEntry.common = common + commonIndex;
            Vector_insert(&((struct inner_cell*)cellBelow)->mthrd.array, 0, makeInner, &innerEntry);
            cellBelow = Vector_get(&((struct inner_cell*)cellBelow)->mthrd.array, 0);
            commonIndex += innerEntry.cints->workmode;
            innerEntry.cints = innerEntry.cints->next;
        }
        free(common);
        Vector_insert(&((struct inner_cell*)cellBelow)->mthrd.array, 0, makeLeafEx, &newLeaf);
        free(newLeaf.uncommon);
        ((struct cell_args*)args)->cints = innerEntry.cints;
        Vector_insert(&((struct inner_cell*)cellBelow)->mthrd.array, 1, makeLeaf, args);
        if (memcmp(((struct leaf_cell*)Vector_get(&((struct inner_cell*)cellBelow)->mthrd.array, 0))->area, ((struct leaf_cell*)Vector_get(&((struct inner_cell*)cellBelow)->mthrd.array, 1))->area, innerEntry.cints->workmode) > 0)
            Vector_swap(&((struct inner_cell*)cellBelow)->mthrd.array, 0, 1);
    }
    return cell;
}

void Fragpath_insert(Fragpath* fragpath, const void* obj , createFunc creator , const void* newObj){
    struct cell_args entry;
    void * temp;
    uint32_t place;
    entry.obj = obj;
    entry.type = creator;
    entry.newObj = newObj;
    entry.tempUn = malloc(fragpath->largestByte);
    instr * firstInstr = entry.cints = Fragpath_iterator_WRITE((struct mthrd_str *)&fragpath->array , fragpath->objStructSizes,entry.tempUn, &place, &temp, obj, &entry.common_count);
    if (firstInstr) {
        if (entry.common_count) Vector_executeFunc(&((struct mthrd_str*)temp)->array, place, expandCell, &entry);
        else Vector_insert(&((struct mthrd_str*)temp)->array, place, makeLeaf, &entry);
#ifdef MultiThread
        mutex_lock(&firstInstr->Mutex);
        ((struct mthrd_str*)temp)->thrdVar -= 1;
        cond_broadcast(&firstInstr->CNDW);
        mutex_unlock(&firstInstr->Mutex);
#endif
    }
    free(entry.tempUn);
    return;
}
void* Fragpath_contains(Fragpath* fragpath, const void* obj){
    void* destination;
    void* data = malloc(fragpath->largestByte);
    void * temp;
    if (Fragpath_iterator_READ((struct mthrd_str *)&fragpath->array, fragpath->objStructSizes , data, &temp, obj) == NULL) destination = ((struct leaf *)temp)->context;
    else destination = NULL;
    free(data);
    return destination;
}
void Fragpath_executeFunc(Fragpath* fragpath, const void* obj , void * (*func)(void * , void *),void * args){
    instr* previous = fragpath->objStructSizes;
    void* data = malloc(fragpath->largestByte);
    void * pinstr[2] = { [0] = NULL , [1] = (struct mthrd_str *)&fragpath->array };
    while (1) {
        uint32_t place;
        instr* next = Fragpath_iterator_ERASE((struct mthrd_str *)pinstr[1], previous,data, &place, pinstr, obj);
        if (pinstr[1] == NULL) {
#ifdef MultiThread
            if (pinstr[0] != NULL) {
                mutex_lock(&previous->Mutex);
                ((struct mthrd_str*)pinstr[0])->thrdVar -= 1;
                cond_broadcast(&previous->CNDW);
                mutex_unlock(&previous->Mutex);
            }
#endif
            break;
        }
        else if (next == NULL) {
            struct disposeCond disposal;
            disposal.freeFunc = func;
            disposal.freeArgs = args;
            disposal.clearAll = 0;
            Vector_executeFunc(&((struct mthrd_str*)pinstr[0])->array, place, executeIn,&disposal);
#ifdef MultiThread
            mutex_lock(&previous->Mutex);
            ((struct mthrd_str*)pinstr[0])->thrdVar -= 1;
            cond_broadcast(&previous->CNDW);
            mutex_unlock(&previous->Mutex);
#endif
            break;
        }
        else {
#ifdef MultiThread
            mutex_lock(&previous->Mutex);
            ((struct mthrd_str*)pinstr[0])->thrdVar -= 1;
            cond_broadcast(&previous->CNDW);
            mutex_unlock(&previous->Mutex);
#endif
            pinstr[0] = NULL;
            previous = next;
        }
    }
    free(data);
    return;
}

struct common {
    LinkedList list;
    struct DS_cpy_args cpyMethod;
};

static void getKeys_Rec(instr* instructions, struct mthrd_str * array, struct common* runcfg){
#ifdef MultiThread
    mutex_lock(&instructions->Mutex);
    while (array->thrdVar & 1)cond_sleep(&instructions->CNDW, &instructions->Mutex);
    array->thrdVar += 2;
    mutex_unlock(&instructions->Mutex);
#endif
    for (uint32_t i = 0, count = Vector_count(&array->array); i < count; i++) {
        void* cell = Vector_get(&array->array, i);
        uint16_t size = instructions->countofjumps;
        {
            instr* tempInst = instructions->next;
            for (uint8_t i = cell_instrcount(cell); i > 1; i--) {
                size += tempInst->countofjumps;
                tempInst = tempInst->next;
            }
        }
        uint32_t offset = 0;
        void* newSource = cell_instrcount(cell) ? ((struct leaf_cell *)cell)->area : ((struct inner_cell*)cell)->area;
        instr* currentInstr = instructions;
        uint16_t setback = 0;
        for (uint16_t k = 0; k < size; k++) {
            if ((k - setback) == currentInstr->countofjumps) {
                setback += currentInstr->countofjumps;
                currentInstr = currentInstr->next;
            }
            void* newDest = (int8_t*)runcfg->cpyMethod.obj + currentInstr->jumps[k - setback].jump;
            void* finalSource = (int8_t*)newSource + offset;
            switch (currentInstr->jumps[k - setback].size) {
                default:*((int8_t*)newDest) = *((int8_t*)finalSource);break;
                case Double:*((int16_t*)newDest) = *((int16_t*)finalSource);break;
                case Quadraple:*((int32_t*)newDest) = *((int32_t*)finalSource);break;
                case DoubleQuadra:*((int64_t*)newDest) = *((int64_t*)finalSource);
            }
            offset += currentInstr->jumps[k - setback].size;
        }
        if (cell_instrcount(cell)) LinkedList_insert(&runcfg->list, -1,DS_cpy, &runcfg->cpyMethod);
        else getKeys_Rec(instructions->next, &((struct inner_cell*)cell)->mthrd, runcfg);
    }
#ifdef MultiThread
    mutex_lock(&instructions->Mutex);
    array->thrdVar -= 2;
    if (!(array->thrdVar >> 1)) cond_broadcast(&instructions->CNDR);
    mutex_unlock(&instructions->Mutex);
#endif
}
void* Fragpath_getKeys(Fragpath* fragpath, uint32_t* count){
    struct common cfg;
    cfg.cpyMethod.bytes = fragpath->bytes;
    cfg.cpyMethod.obj = malloc(cfg.cpyMethod.bytes);
    LinkedList_create(&cfg.list);
    getKeys_Rec(fragpath->objStructSizes, (struct mthrd_str *)&fragpath->array, &cfg);
    free(cfg.cpyMethod.obj);
    void* result = malloc(fragpath->bytes * (*count = LinkedList_count(&cfg.list)));
    void* parser = result;
    while (LinkedList_count(&cfg.list) > 0) {
        memcpy(parser, LinkedList_get(&cfg.list, 0), fragpath->bytes);
        parser = (int8_t*)parser + fragpath->bytes;
        LinkedList_executeFunc(&cfg.list,0, DS_nullify, NULL);
    }
    return result;
}
void Fragpath_swap(Fragpath* fragpath, const void* obj1, const void* obj2){
    void * path1;
    void* data = malloc(fragpath->largestByte);
    if (Fragpath_iterator_READ((struct mthrd_str *)&fragpath->array, fragpath->objStructSizes, data, &path1, obj1) == NULL) {
        void * path2;
        if (Fragpath_iterator_READ((struct mthrd_str *)&fragpath->array, fragpath->objStructSizes,data, &path2, obj2) == NULL) {
            void* temp = ((struct leaf *)path1)->context;
            ((struct leaf*)path1)->context = ((struct leaf*)path2)->context;
            ((struct leaf*)path2)->context = temp;
        }
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

static void* setInstr(const void* data){
    uint16_t count = ((semi_instr*)data)->countofjumps, i = 0;
    struct pure_instr* result = (struct pure_instr*)malloc(sizeof(struct pure_instr) + (count * sizeof(jumpInstruction)));
    jumpInstruction* dest = result->jumps, * src = ((semi_instr*)data)->jumps;
    while (i < count) {
        dest[i].jump = src[i].jump;
        dest[i].size = src[i].size;
        i += 1;
    }
    result->countofjumps = count;
    result->workmode = ((semi_instr*)data)->workmode;
    return result;
}
static instr* Fragpath_init(LinkedList* const actualbytes, uint16_t* const restrict largeOut){
    LinkedList steplist; 
    LinkedList_create(&steplist);
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
        struct DS_cpy_args cpyArg;
        cpyArg.bytes = sizeof(struct jumpcmd);
        cpyArg.obj = &currJumpOp;
        LinkedList_create(&command);
        for (uint8_t i = 0 , temp = 1; i < segcount; ++i) while (bytesPerSegm[i]) {
            uint16_t maxStep = bytes - ((temp < bytes) * (bytes - temp));
            bytes -= maxStep;
            temp = temp << 1;
            *largeOut = *largeOut + (*largeOut < maxStep) * (maxStep - *largeOut);
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
                        LinkedList_insert(&command,-1, DS_cpy, &cpyArg);
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
                        LinkedList_insert(&command,-1, DS_cpy, &cpyArg);
                    }
                    if (currentJump == 6) {
                        currentJump -= 2;
                        currJumpOp.jumpSize = Double;
                        currJumpOp.segment = temploc->segment;
                        LinkedList_insert(&command,-1, DS_cpy, &cpyArg);
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
                LinkedList_insert(&steplist,-1 , setInstr, &data);
                free(data.jumps);
            }
            LinkedList_clear(&command, DS_nullify, NULL);
        }
        free(bytesPerSegm);
        LinkedList_destroy(actualbytes,DS_nullify,NULL);
        LinkedList_destroy(&command,DS_nullify,NULL);
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
            cond_init(&(*dest)->CNDR);
            cond_init(&(*dest)->CNDW);
#endif
            LinkedList_executeFunc(&steplist,0, DS_nullify, NULL);
            dest = &(*dest)->next;
        } while (LinkedList_count(&steplist) > 0);
        (*dest) = NULL;
    }
    return result;
}
void Fragpath_create(Fragpath* fragpath,const seginfo* restrict objectStruct,uint8_t arrayCount){
    uint16_t objBytes;
    {
        LinkedList map; LinkedList_create(&map);
        struct DS_cpy_args cpArgs;
        cpArgs.bytes = sizeof(seginfo);
        uint8_t f = 0;
        {
            seginfo newseg , *currentSeg;
            currentSeg = NULL;
            cpArgs.obj = &newseg;
            newseg[1] = objectStruct[f][1];
            do {
                newseg[0] = objectStruct[f][0];
                if (currentSeg == NULL || newseg[0] != (*currentSeg)[0] + (*currentSeg)[1]) {
                    LinkedList_insert(&map, -1, DS_cpy, &cpArgs);
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
            cpArgs.obj = &finalseg;
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
                        LinkedList_insert(&map, -1, DS_cpy, &cpArgs);
                        currentTail = (seginfo*)LinkedList_get(&map, -1);
                    }
                }
            }
        }
        fragpath->objStructSizes = Fragpath_init(&map, &fragpath->largestByte);
        Vector_create(&fragpath->array,1);
    }
#ifdef MultiThread
    fragpath->thrdVar = 0;
#endif
    fragpath->bytes = arrayCount * objBytes;
    return;
}
void Fragpath_destroy(Fragpath* fragpath, void* (*func)(void*, void*), void* args){
    struct disposeCond disposal;
    disposal.freeFunc = func;
    disposal.freeArgs = args;
    disposal.clearAll = 1;
    Vector_destroy(&fragpath->array,executeIn,&disposal);
    for (instr* place = fragpath->objStructSizes, *nextPlace; place != NULL; place = nextPlace) {
        nextPlace = place->next;
#ifdef MultiThread
        mutex_destroy(&place->Mutex);
        cond_destroy(&place->CNDR);
        cond_destroy(&place->CNDW);
#endif
        free(place);
    }
    return;
}
void Fragpath_clear(Fragpath* fragpath, void* (*func)(void*, void*), void* args){
    struct disposeCond disposal;
    disposal.freeFunc = func;
    disposal.freeArgs = args;
    disposal.clearAll = 1;
    Vector_clear(&fragpath->array,executeIn, &disposal);
    return;
}