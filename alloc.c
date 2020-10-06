/**
 * OVERALL IDEA:
 * I decided that this time I am going to use a free list, and a alloc list
 * This means that each metadata tag, while large, will be easy to maintain, for a dumbass like myself.
 * 
 * 
 * TODO:
 * FreeList insert
 * coalesce
 * malloc
 * callloc
 * realloc
 * TESTSSSSSSSSS
 * freelist split
 * TEST
 * OPTMIZE
 * 
 * 
 */
#define DEBUGME    0

#define BAD_ADD    0xBADC0FFEE0DDF00D
#define GOOD_ADD   0xCAFEBABED00D900D
#define RIGHT_END  0xD00D999999999999
#define LEFT_END   0xBABE000000000000
#define SPLIT      0xBABE
#define MINALLOC   512
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>

typedef struct meta_data_t {
    struct meta_data_t *PREV;
    struct meta_data_t *fprev;  
    size_t size;
    int isfree;
    struct meta_data_t *fnext; 
    struct meta_data_t *NEXT;
 } meta_t;

// HEAD and TAIL pointers for the memory layout list
static meta_t* memHEAD = NULL;
static meta_t* memTAIL = NULL;
// Head and tail for the free list
static meta_t* fhead = NULL; 
static meta_t* ftail = NULL;
// Number for alloced slots for debugging
static ssize_t alloc_count = 0;
static ssize_t free_count = 0;
static size_t malloc_count = 0;
static int SPLIT_TEST = 1;
void* coalesce_blocks(meta_t* center);
void block_split(void* ptr, ssize_t size);
void debug_realloc(void *ptr, ssize_t size);

void print_heap(){
    if(DEBUGME == 0) return;
    if (memHEAD != NULL) {
        meta_t* temp = memHEAD;
        size_t count = 0;
        fprintf(stderr, "-------------------------HEAP START sizeof(meta_t*): %zu-------------------------\n", sizeof(meta_t));
        while(temp->NEXT != NULL){
            fprintf(stderr, "|count:%zu, current: %p, memPREV: %p, size: %zu, isfree: %d, memNext: %p|\n", count, temp, temp->PREV, temp->size, temp->isfree, temp->NEXT);
            temp = temp->NEXT;
            count++;
        }
        fprintf(stderr, "|count:%zu, current: %p, memPREV: %p, size: %zu, isfree: %d, memNext: %p|\n", count, temp, temp->PREV, temp->size, temp->isfree, temp->NEXT);
        if(temp != memTAIL){
            temp = memTAIL;
            fprintf(stderr, "\n!!!!!!!!!!THE ROOF IS ON FIRE!!!!!!!!!!\n\nTAIL:\n| count:%zu, current: %p, memPREV: %p, size: %zu, isfree: %d, memNext: %p|\n", count, temp, memTAIL->PREV, memTAIL->size, memTAIL->isfree, memTAIL->NEXT);
        }
        fprintf(stderr, "---HEAP END---\n");
    }

}

void print_free(){
    if(DEBUGME == 0) return;
    if (fhead != NULL) {
        meta_t* temp = fhead;
        size_t count = 0;
        fprintf(stderr, "---FREE START---\n");
        while(temp->fnext != NULL){
            fprintf(stderr, "|count:%zu, current: %p, fprev: %p, size: %zu, isfree: %d, fnext: %p|\n", count, temp, temp->fprev, temp->size, temp->isfree, temp->fnext);
            temp = temp->fnext;
            count++;
        }
        fprintf(stderr, "|count:%zu, current: %p, fprev: %p, size: %zu, isfree: %d, fnext: %p|\n", count, temp, temp->fprev, temp->size, temp->isfree, temp->fnext);
        if(temp != ftail){
            temp = ftail;
            fprintf(stderr, "\n!!!!!!!!!!THE FREE MARKET HAS CRASHED!!!!!!!!!!\nTAIL:\n|count:%zu, current: %p, fprev: %p, size: %zu, isfree: %d, fnext: %p|\n", count, temp, ftail->fprev, ftail->size, ftail->isfree, ftail->fnext);
        }
        fprintf(stderr, "------------------------------------FREE END!------------------------------------\n");
    }

}

void debug_malloc(size_t size){
    fprintf(stderr, ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> Malloc called with size: %zu\n", size);
    fprintf(stderr, "Malloc count is now: %zu\n", malloc_count);
}


void debug_realloc(void* ptr, ssize_t size){
    meta_t* entry = ((meta_t*)ptr) - 1;
    ssize_t old_size = entry->size;
    ssize_t diff = old_size - size;
    fprintf(stderr, "\nRealloc called with new_size: %zu, old_size: %zu, diff: %zd, from ptr: %p\n", size, old_size, diff, entry);
}

void debug_free(void* ptr){
    meta_t* entry = ((meta_t*)ptr) - 1;
    fprintf(stderr, "\nFREE called with size: %zu, from ptr: %p\n", entry->size, entry);
}
/**
 * Adds a value to the free list. This is by pushing on top
 * For right now we are going sort the list based off size
 * 
 * SORT: smallest to largest
 * ASK: What about for first iter? 
 * TODO: Make it so it becomes sorted
 * 
 *   // This will insert the entry before an entry of greater than or equal size
 *   // Cases to consider:
 *   // Left side is a end
 *   // Right side is an end
 *   // Add to front
 *   // Add to end
 *   // insert in the middle
 */
void free_list_insert(meta_t* new_entry) {
    new_entry->isfree = 1;
    free_count++;
    /** CASE 0: LIST IS UNINITALIZED OR EMPTY */
    if(fhead == NULL){ 
        // Set the canaries
        new_entry->fprev = NULL;
        new_entry->fnext = NULL;
        // Update the ends
        fhead = new_entry;
        ftail = new_entry;
        return;
    }
    /** CASE 1: ADD ONTO THE FRONT (LEFT SIDE/HEAD) OF THE LIST */
    if(new_entry->size <= fhead->size) {
        // Update new_entry pointers to be fHEAD
        new_entry->fprev = NULL;
        new_entry->fnext = fhead;
        // Updated the fhead to new_entry
        fhead->fprev = new_entry;
        fhead = new_entry;
        return;
    }
    /** CASE 2: INSERT INTO THE MIDDLE OF THE LIST */
    /** NOTICE: Iterating over head, till the second to last element */
    meta_t* temp = fhead;
    while(temp->fnext != NULL) {
        if(new_entry->size <= temp->size) { // Regular case for insertings
            meta_t* temp_left = temp->fprev;
            // Handle the left hand node connection
            temp_left->fnext = new_entry;
            new_entry->fprev = temp_left;
            // Handle the right hand node connection
            temp->fprev = new_entry;
            new_entry->fnext = temp;
            return;
        }
        temp = temp->fnext;
    }
    /** CASE 3: ADD ONTO THE END (RIGHT SIDE/TAIL) OF THE LIST */
    if(temp->fnext == NULL) { 
        // Adjust the new_entry data to be TAIL
        new_entry->fprev = temp;
        new_entry->fnext = NULL;
        // Adjust the current TAIL to attach to new_entry
        temp->fnext = new_entry;
        // Set the new TAIL
        ftail = new_entry;
        return;
    }
    write(0, "INSERT to the free list fell through! ENCOUNTERD CRAZY ERROR! EXITING!!!!\n", 74);
    exit(0);
}

/**
 * 
 * This checks the free list, to see if we can reuse a block
 * for a new alloc.
 * 
 * Future: implement edge case so that we can avoid scanning list everytime
 */
void* free_list_check(size_t new_size) {
    /** CASE 0: The free list is uninitalized/EMPTY */
    if(fhead == NULL){ 
        return NULL;
    }
    /** CASE 1: The head is larger than the new_size */
    if(new_size <= fhead->size) {
        void* ptr = ((meta_t*)fhead) + 1;  // Return the ptr to payload for head
        // ASK: When we update the head? Does this cause any issues?
        meta_t* temp = fhead->fnext; // Get the next element in the free list
        if(temp != NULL) {       // Check to make sure that the next element isn't NULL. I.E: The free list only contains one value 
            temp->fprev = NULL;  // Update the next element so it points to NULL
            fhead->fnext = NULL; // take the current head, set both prev, next to NULL to remove it from the list
            fhead->fprev = NULL;
            fhead = temp;        // Update so that the next free list element is the HEAD
        } else{
            // ASK: Do we want to check if ftail is NULL?
            fhead = NULL; // Free list is now empty in this edge case
            ftail = NULL;
        }
        free_count--;

        meta_t* result = ((meta_t*)ptr) - 1;
        result->isfree = 0;

        return ptr;
    }
    /** CASE 2: Scan the list for an open spot */
    meta_t* curr = fhead;
    while(curr->fnext != NULL) {
        if(new_size <= curr->size) {
            meta_t* after = curr->fnext;
            meta_t* before = curr->fprev;
            // Update the surrounding values in the free list
            before->fnext = after;
            after->fprev = before;
            // Remove the current value from the free list
            curr->fprev = NULL;
            curr->fnext = NULL;
            // Get the pointer to the payload so we can return it
            //void* res = (void*) ( ((char)curr) + sizeof(meta_t) );
            void* ptr = ((meta_t*)curr) + 1;
            free_count--;

            meta_t* result = ((meta_t*)ptr) - 1;
            result->isfree = 0;

            return ptr;
        }
        curr = curr->fnext;
    }
    // ASK: Do we need to check if the TAIL is <= new_size
    if(new_size <= ftail->size){
        curr = ftail;
        void* ptr = ((meta_t*)curr) + 1;
        if(ftail->fprev != NULL) {
            meta_t* buf = ftail->fprev;
            curr->fprev = NULL;
            curr->fnext = NULL;
            buf->fnext = NULL;
            ftail = buf;

            meta_t* result = ((meta_t*)ptr) - 1;
            result->isfree = 0;
            free_count--;
            return ptr;
        }
    }
    return NULL;   
}

/**
 * Adds a value to the mem list. This is by appending on RHS, TAIL
 *
 * ASK: What about for first iter? 
 * ||HEAD|| <-- Prev|payload| --> NEXT   || TAIL ||
 */
void mem_list_append(meta_t* new_entry) {
    if(memHEAD == NULL) { // This is for the first malloc
        // Set the canaries
        new_entry->PREV = NULL;
        new_entry->NEXT = NULL;
        // Update the ends
        memHEAD = new_entry;
        memTAIL = new_entry;
        // This is in the metadata for debug reasons
        alloc_count++;
        return;
    } else {
        // Setup the new entry
        new_entry->PREV = memTAIL;
        new_entry->NEXT = NULL;
        // update the current end to point to new_entry
        memTAIL->NEXT = new_entry;
        // now set the tail to the new_entry;
        memTAIL = new_entry;
        // Set loc for debug
        alloc_count++;
        return;
    }
}




// ALLOCATOR MAIN FUNCTIONS

/**
 * Allocate space for array in memory
 *
 * Allocates a block of memory for an array of num elements, each of them size
 * bytes long, and initializes all its bits to zero. The effective result is
 * the allocation of an zero-initialized memory block of (num * size) bytes.
 *
 * @param num
 *    Number of elements to be allocated.
 * @param size
 *    Size of elements.
 *
 * @return
 *    A pointer to the memory block allocated by the function.
 *
 *    The type of this pointer is always void*, which can be cast to the
 *    desired type of data pointer in order to be dereferenceable.
 *
 *    If the function failed to allocate the requested block of memory, a
 *    NULL pointer is returned.
 *
 * @see http://www.cplusplus.com/reference/clibrary/cstdlib/calloc/
 */
void *calloc(size_t num, size_t size) {
    size_t payload_size = (num * size);
    void* ptr = malloc(payload_size);
    memset(ptr, 0, payload_size);
    return ptr;
}

/**
 * Allocate memory block
 *
 * Allocates a block of size bytes of memory, returning a pointer to the
 * beginning of the block.  The content of the newly allocated block of
 * memory is not initialized, remaining with indeterminate values.
 *
 * @param size
 *    Size of the memory block, in bytes.
 *
 * @return
 *    On success, a pointer to the memory block allocated by the function.
 *
 *    The type of this pointer is always void*, which can be cast to the
 *    desired type of data pointer in order to be dereferenceable.
 *
 *    If the function failed to allocate the requested block of memory,
 *    a null pointer is returned.
 *
 * @see http://www.cplusplus.com/reference/clibrary/cstdlib/malloc/
 * 
 * 
 * 
 * First iteration: Only add the sbrk and add to the list
 */
void *malloc(size_t size) {
    if(DEBUGME == 1) debug_malloc(size);
    if(malloc_count == 1){
        if(2000000000 < size){
            SPLIT_TEST = 1;
        }
    }
    malloc_count++;

    /** CASE 0: Malloc of size 0 reguested */
    if(size == 0){
        return NULL;
    }
    //size = 4*((size+3)/4);
    /** CASE 1: The free list contains an okay value */
    size_t total = (sizeof(meta_t) + size);
    void* ptr = free_list_check(size);

    if(ptr != NULL) {
        print_heap();
        print_free();
        /** CASE 1.5: The Block given is extremely large in comparison */
        meta_t* left_split = ((meta_t*)ptr) - 1;
        ssize_t diff = left_split->size - size;
        ssize_t split_size = diff - sizeof(meta_t);
        if(SPLIT_TEST == 1){
            if(diff > 0){
                if(split_size >= 536870912){
                    if(DEBUGME == 1) write(2, "We will attempt to split\n", 26);
                    block_split(ptr, size);
                }
            }
        }
        // ASK: If I split, should I coalesce at the same time?
        if(DEBUGME == 1) write(2, "MALLOC End with split or ptr\n", 30); 
        return ptr;
    }

    /** CASE 2: There was no free block big enough, so we must coalesce */
    if(fhead != NULL){
        if(DEBUGME == 1) write(2, "We will attempt to coalesce\n", 29);
        meta_t* temp = memHEAD;
        if(fhead != ftail){
            while(temp->NEXT != NULL) {
                coalesce_blocks(temp);
                if(temp->NEXT == NULL){
                    break;
                }
                temp = temp->NEXT;
            }

            ptr = free_list_check(size);
            if(ptr != NULL) {
                //write(2, "We have successfully coalesced!\n", 33);
                print_heap();
                print_free();
                if(DEBUGME == 1) write(2, "MALLOC with coalesce End\n", 26); 
                return ptr;
            }
        }
    }

    /** CASE 2: We need to sbrk, as there are no new blocks.*/
    void* out = sbrk(total);
    if(out == (void*) -1){
        write(2, "\nfuck\n", 8);
        return NULL;
    }

    meta_t* entry = (meta_t*) out;
    entry->size = size;
    entry->fprev = NULL;
    entry->fnext = NULL;
    entry->isfree = 0;
    mem_list_append(entry);
    ptr = (void*) (((meta_t*)entry) + 1);
    if(DEBUGME == 1) {
        print_heap();
        print_free();
    }
    if(DEBUGME == 1) write(2, "MALLOC End with sbrk\n", 22); 
    return ptr;
    
}

/**
 * Deallocate space in memory
 *
 * A block of memory previously allocated using a call to malloc(),
 * calloc() or realloc() is deallocated, making it available again for
 * further allocations.
 *
 * Notice that this function leaves the value of ptr unchanged, hence
 * it still points to the same (now invalid) location, and not to the
 * null pointer.
 *
 * @param ptr
 *    Pointer to a memory block previously allocated with malloc(),
 *    calloc() or realloc() to be deallocated.  If a null pointer is
 *    passed as argument, no action occurs.
 */
void free(void *ptr) {
    //meta_t* new_entry = (meta_t*) ( ((char)ptr) - sizeof(meta_t) );
    //write(1, "```````````````` Free is called ````````````````\n", 50);
    if(DEBUGME == 1) debug_free(ptr);
    meta_t* new_entry = ((meta_t*)ptr) - 1;
    //fprintf(stderr, "\n---------------Free is called: %p ---------------\n", new_entry);
    free_list_insert(new_entry);
    print_free();
    //sleep(1);
}

/**
 * Reallocate memory block
 *
 * The size of the memory block pointed to by the ptr parameter is changed
 * to the size bytes, expanding or reducing the amount of memory available
 * in the block.
 *
 * The function may move the memory block to a new location, in which case
 * the new location is returned. The content of the memory block is preserved
 * up to the lesser of the new and old sizes, even if the block is moved. If
 * the new size is larger, the value of the newly allocated portion is
 * indeterminate.
 *
 * In case that ptr is NULL, the function behaves exactly as malloc, assigning
 * a new block of size bytes and returning a pointer to the beginning of it.
 *
 * In case that the size is 0, the memory previously allocated in ptr is
 * deallocated as if a call to free was made, and a NULL pointer is returned.
 *
 * @param ptr
 *    Pointer to a memory block previously allocated with malloc(), calloc()
 *    or realloc() to be reallocated.
 *
 *    If this is NULL, a new block is allocated and a pointer to it is
 *    returned by the function.
 *
 * @param size
 *    New size for the memory block, in bytes.
 *
 *    If it is 0 and ptr points to an existing block of memory, the memory
 *    block pointed by ptr is deallocated and a NULL pointer is returned.
 *
 * @return
 *    A pointer to the reallocated memory block, which may be either the
 *    same as the ptr argument or a new location.
 *
 *    The type of this pointer is void*, which can be cast to the desired
 *    type of data pointer in order to be dereferenceable.
 *
 *    If the function failed to allocate the requested block of memory,
 *    a NULL pointer is returned, and the memory block pointed to by
 *    argument ptr is left unchanged.
 *
 * @see http://www.cplusplus.com/reference/clibrary/cstdlib/realloc/
 * 
 * 
 * TODO: 
 *      Implement Coalescing
 *      Implement Splitting
 * 
 */
void *realloc(void *ptr, size_t size) {
    //write(1, "Realloc is called\n", 18);
    if(DEBUGME == 1) debug_realloc(ptr, size);
    /** CASE 0: The user wants to realloc to 0 */
    if(size == 0) {
        free(ptr);  
        if(DEBUGME == 1) write(2, "Realloc End\n", 13);  
        return NULL;
    }

    meta_t* entry = ((meta_t*)ptr) - 1;
    size_t old_size = entry->size;
    //size_t min_size = (MINALLOC + sizeof(meta_t));
    /** CASE 1: The user passes a NULL for ptr */
    if(ptr == NULL) {
        void* res = malloc(size);
        if(DEBUGME == 1) write(2, "Realloc End\n", 13);  
        return res;
    }

    if(size == old_size){
        if(DEBUGME == 1) write(2, "Realloc End\n", 13);  
        return ptr;
    }

    if (size < old_size) {
        if(DEBUGME == 1) write(2, "Realloc End\n", 13);  
        return ptr;
    }

    //size_t requested_size = 8*((size+7)/8);
    /** CASE 4: We have to malloc because there are no free blocks which work. */
    // ASK: Is this in efficent because malloc will handle blocks?
    void* new_alloc = malloc(size);
    meta_t* test = ((meta_t*)new_alloc) - 1;
    if(test->size > size){
        //write(0, "wtf\n", 4);
        memset(new_alloc, 0xDEADC0DE, old_size);
        memcpy(new_alloc, ptr, old_size);
        free(ptr);
        if(DEBUGME == 1) write(2, "Realloc End\n", 13);  
        return new_alloc;
        return NULL;
    }
    if(size == test->size){
        //write(0, "confusion\n", 11);
        memcpy(new_alloc, ptr, old_size);
        free(ptr);
        if(DEBUGME == 1) write(2, "Realloc End\n", 13);  
        return new_alloc;
    } else{
        if(DEBUGME == 1) write(2, "Realloc End\n", 13);  
        return NULL;
    }
}


void free_list_unlink(meta_t* entry){
    entry->fprev = NULL;
    entry->fnext = NULL;
    free_count--;
    return;
}

void mem_list_unlink(meta_t* entry){
    entry->PREV = NULL;
    entry->NEXT = NULL;
    alloc_count--;
    return;
}
/** This will handle the removal of entries from the free_list */
void free_list_remove(meta_t *entry) {
    if(fhead == NULL){
        return;
    }

    /** CASE 0: Entry is equal to fhead */
    if(entry == fhead) { 
        /** CASE 0.0: This is the general case for removing from the front. */
        if(entry->fnext != NULL) {
            meta_t* after = entry->fnext;
            after->fprev = NULL;
            fhead = after;
            free_list_unlink(entry);
            return;
        }
        if (entry == ftail) { 
        /** CASE 0.5: This is the special case for if the fhead == ftail */ 
            fhead = NULL;
            ftail = NULL;
            if(free_count == -2){
                //write(1, "Free count was gonna go negative, fix this!\n", 44);
                //exit(69);
            }
            free_list_unlink(entry);
            return;
        }
    }
    /** CASE 1: Entry is equal to ftail */
    if(entry == ftail) {
        meta_t* before = entry->fprev;
        before->fnext = NULL;
        ftail = before;
        free_list_unlink(entry);
        return;
    }

    /** 2.0: Remove left link */
    if(entry->fprev == NULL){
        if(entry->fnext == NULL){
            free_list_unlink(entry);
            return;
        } else{
            write(1,"Do I really need to consider this?\n", 36);
        }
    }

    /** CASE 2: Entry is in the middle of the list */
    meta_t* before = entry->fprev;
    meta_t* after = entry->fnext;
    before->fnext = after;
    after->fprev = before;
    free_list_unlink(entry);
    return;
    
}


/** This will handle the removal of entries from the alloc_list */
void mem_list_remove(meta_t *entry) {
    if(memHEAD == NULL){
        return;
    }
    /** CASE 0: Entry is equal to memHEAD */
    if(entry == memHEAD) { 
        /** CASE 0.0: This is the general case for removing from the front. */
        if(entry->NEXT != NULL) {
            meta_t* after = entry->NEXT;
            after->PREV = NULL;
            memHEAD = after;
            mem_list_unlink(entry);
            return;
        }
        if(entry == memTAIL) { 
        /** CASE 0.5: This is the special case for if the memHEAD == memTAIL */
            memHEAD = NULL;
            memTAIL = NULL;
            if(alloc_count == 0){
                write(1, "Alloc count was gonna go negative, fix this!\n", 46);
                exit(69);
            }
            mem_list_unlink(entry);
            return;
        }
    }
    /** CASE 1: Entry is equal to memTAIL */
    if(entry == memTAIL) {
        meta_t* before = entry->PREV;
        before->NEXT = NULL;
        memTAIL = before;
        mem_list_unlink(entry);
        return;
    }

    if(entry->PREV == NULL){
        if(entry->NEXT == NULL){
            mem_list_unlink(entry);
            return;
        } else{
            write(1,"Do I really need to consider this?\n", 36);
        }
    }
    /** CASE 2: Entry is in the middle of the list */
    meta_t* before = entry->PREV;
    meta_t* after = entry->NEXT;
    before->NEXT = after;
    after->PREV = before;
    mem_list_unlink(entry);
    return;   
}



/**
 * Coalesce free blocks function
 * 
 * This will coealescing the surrounding blocks using the ALLOC_LIST, which
 * is just the list of all allocations.
 * 
 * This function has the power to only remove blocks from the ALLOC_LIST, not add new ones
 * This function will also decrement the alloc_count based off of how many blocks coalesced
 * 
 * It will check the PREV and NEXT blocks in the allocatino list based off if their payloads contain the CANARY FREE_BIRD
 * 
 * ASK: Is there anycase where the next value in the ALLOC_LIST is not directly next in memory?
 *      THIS WILL OCCUR DURING BLOCK SPLITTING, HOWEVER< WE ARE COALESCING FIRST
 * 
 * ASK: WE WILL HAVE TO UPDATE THE FREE LIST, AS WE WILL BE REMOVING VALUES FROM IT
 * 
 * ASK: Do we save that much time with the canary?
 * 
 * NOTE: This means that there will be two more functions to add with this addtion
 *      FREE LIST REMOVE -> To remove the surrounding free list values
 *      ALLOC_LIST REMOVE -> to rmeove the surrounding values
 * 
 * @param meta_t *center
 *      meta_t pointer to the currently freeing block
 *      If this is the start of the heap we only coalesce the NEXT block
 *      If this is the end of the heap we only coalesce the PREV block
 *      If this is in the middle, we coalesce both PREV, and NEXT.
 * 
 * @return void* res
 *      The pointer returned is the resuling block from the coalescing
 *      It can either be:
 *          No coalescing, returns original center payload
 *          Right coalescing, returns original center, with a new extended size to the right
 *          Left coalescing, returns the pointer the the block before it in memory
 * 
 */ 
// NOTE: You will have to remove the center from the free list too, that way the new value is resorted in the list
void* coalesce_blocks(meta_t *center) {
    if(center->isfree == 0){
        //write(1, "Coalescing is exiting fail\n", 28);
        return NULL;
        //exit(69);
    }
    meta_t* result = center;

    int flag = 0;
    size_t center_size = center->size;
    free_list_remove(center);

    // Check the right block
    if(center->NEXT != NULL) {
        // Get the right pointer
        meta_t* RIGHT = center->NEXT;
        if(RIGHT->isfree == 1){
            size_t addition = RIGHT->size + sizeof(meta_t);
            free_list_remove(RIGHT);
            mem_list_remove(RIGHT);
            center->size += addition;
            result = center;
            flag++;
        }
    }

    // Check the left block
    if(center->PREV != NULL) {
        meta_t* LEFT = center->PREV;
        if(LEFT->isfree == 1) {
            free_list_remove(LEFT);
            mem_list_remove(center);
            LEFT->size += center_size + sizeof(meta_t);
            result = LEFT;
        }
    }

    free_list_insert(result);
    return (void*) (((meta_t*)result) + 1);
}

void block_split(void* ptr, ssize_t size) {
    meta_t* left_split = ((meta_t*)ptr) - 1;
    ssize_t diff = left_split->size - size;
    if(diff <= 0) return;
    size_t split_size = diff - sizeof(meta_t);
    meta_t* right_split = (meta_t*)(((void*)ptr) + size); // ASK: Do we need to do a +1?
    if(DEBUGME == 1) fprintf(stderr, "*************************Size Requested: %zu, Size difference: %zu***********************\n", size, diff);
     // ASK: What about if the mem list values are head/tails
    if(left_split->NEXT != NULL){

        right_split->PREV = left_split;
        right_split->fprev = NULL;
        right_split->size = split_size;
        right_split->isfree = 1;
        right_split->fnext = NULL;
        right_split->NEXT = left_split->NEXT;

        left_split->NEXT = right_split;
        left_split->size = size;

        alloc_count++;
        free_list_insert(right_split);
        return;

    }else if(left_split == memTAIL){

            right_split->PREV = NULL;
            right_split->fprev = NULL;
            right_split->size = split_size;
            right_split->isfree = 1;
            right_split->fnext = NULL;
            right_split->NEXT = NULL;
            left_split->size = size;
          
            mem_list_append(right_split);
            free_list_insert(right_split);
            return;
    }

    if(left_split->NEXT == NULL){
        write(2, "The left_split NEXT is NULL! But it is not memtail!!!!\n", 56);
    }
    write(2, "Werid we are at the end of the split function without returning!\n", 66);
    if(DEBUGME == 1) {
        print_heap();
        print_free();
        fprintf(stderr, "*************************END SPLIT***********************\n");
    }

}
