#include <stdio.h>
#include <stdlib.h>

typedef unsigned char            ArenaFlag;
typedef struct ArenaBlockHeader  ArenaBlockHeader;
typedef struct ArenaAllocator    ArenaAllocator;

#define ARENA_ENABLE_DEBUG 1

/*
    when allocate a block, it can only be those 3 status:

    1. only one pointer to take the whole block,
    2. multi pointers split this block,
    3. no usage.

    if case 1 is fit, then if that block is no need to use, we can
    consider it as a new block, and reuse it in case 1 or case 2.
*/
#define ARENA_FLAG_ONLY_ONE      0
#define ARENA_FLAG_MULTI_PARTS   1
#define ARENA_FLAG_NO_USE        2

struct ArenaBlockHeader {
    size_t used;
    size_t capacity;
    ArenaFlag flag;
    ArenaBlockHeader* next;
};

struct ArenaAllocator {
    ArenaBlockHeader* head;
    size_t blockSize;
    size_t blockNum;
};

ArenaAllocator* arena_create(size_t blockSize) {
    ArenaAllocator* arena = (ArenaAllocator*)malloc(sizeof(ArenaAllocator));

    if (arena == NULL) {
        return NULL;
    }

    arena->head = (ArenaBlockHeader*)malloc(sizeof(ArenaBlockHeader) + blockSize);
    if (arena->head == NULL) {
        free(arena);
        return NULL;
    }

    arena->blockSize = blockSize;
    arena->head->capacity = blockSize;
    arena->head->flag = ARENA_FLAG_NO_USE;
    arena->head->used = 0;
    arena->head->next = NULL;
    arena->blockNum = 1;

    #if ARENA_ENABLE_DEBUG
    printf("default block size is %d\n\n", blockSize);
    #endif

    return arena;
}

void arena_free(ArenaAllocator* arena) {
    ArenaBlockHeader* cursor;

    if (arena != NULL) {
        cursor = arena->head;

        while (cursor != NULL) {
            arena->head = cursor->next;
            free(cursor);
            cursor = arena->head;
        }

        free(arena);
    }
}

ArenaBlockHeader* arena_create_new_block(ArenaAllocator* arena, size_t size, size_t used, ArenaFlag flag) {
    ArenaBlockHeader* newBlock = (ArenaBlockHeader*)malloc(sizeof(ArenaBlockHeader) + size);
    
    if (newBlock != NULL) {
        newBlock->capacity = size;
        newBlock->flag = flag;
        newBlock->used = used;
        newBlock->next = arena->head;
        arena->head = newBlock;

        arena->blockNum += 1;
    }

    return newBlock;
}

void* arena_malloc(ArenaAllocator* arena, size_t size) {
    ArenaBlockHeader* cursor = arena->head;
    ArenaBlockHeader* newBlock;

    while (cursor != NULL) {
        if (cursor->flag != ARENA_FLAG_ONLY_ONE && cursor->used + size <= cursor->capacity) {
            cursor->used += size;

            if (size == arena->blockSize) {
                cursor->flag = ARENA_FLAG_ONLY_ONE;
            }
            else {
                cursor->flag = ARENA_FLAG_MULTI_PARTS;
            }

            #if ARENA_ENABLE_DEBUG
            printf("want %d, find a block which has enough space, use it\n", size);
            #endif

            return (void*)((char*)(cursor + 1) + cursor->used - size);
        }

        cursor = cursor->next;
    }

    /* if can't find, create a new block. */
    if (size < arena->blockSize) {
        newBlock = arena_create_new_block(arena, arena->blockSize, size, ARENA_FLAG_MULTI_PARTS);

        #if ARENA_ENABLE_DEBUG
        printf("want %d, no block is fit, allocate a new %d size block, set `multi`\n", size, arena->blockSize);
        #endif
    }
    else {
        newBlock = arena_create_new_block(arena, size, size, ARENA_FLAG_ONLY_ONE);

        #if ARENA_ENABLE_DEBUG
        printf("want %d, no block is fit, allocate a new %d size block, set `only one`\n", size, size);
        #endif
    }

    return newBlock != NULL ? (void*)(newBlock + 1) : NULL;
}

/*
    try to recycle memory allocated by arena allocator.
*/
void arena_recycle(ArenaAllocator* arena, void* memory, size_t capacity) {
    ArenaBlockHeader* header = (ArenaBlockHeader*)memory - 1;

    if (capacity >= arena->blockSize && header->flag == ARENA_FLAG_ONLY_ONE) {
        header->flag = ARENA_FLAG_NO_USE;
        header->used = 0;

        #if ARENA_ENABLE_DEBUG
        printf("recycle %d size block\n", header->capacity);
        #endif
    }
}

int main() {
    void* test_for_recycle;
    ArenaAllocator* arena = arena_create(12);

    arena_malloc(arena, 2);
    test_for_recycle = arena_malloc(arena, 13);
    arena_malloc(arena, 4);
    arena_malloc(arena, 6);
    arena_malloc(arena, 7);
    arena_malloc(arena, 14);
    arena_recycle(arena, test_for_recycle, 13);
    arena_malloc(arena, 13);

    /*
        default block size is 12

        want 2, find a block which has enough space, use it
        want 13, no block is fit, allocate a new 13 size block, set `only one`
        want 4, find a block which has enough space, use it
        want 6, find a block which has enough space, use it
        want 7, no block is fit, allocate a new 12 size block, set `multi`
        want 14, no block is fit, allocate a new 14 size block, set `only one`
        recycle 13 size block
        want 13, find a block which has enough space, use it
    */

    arena_free(arena);
}
