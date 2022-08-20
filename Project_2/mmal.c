//xosker03
/**
 * Implementace My MALloc
 * Demonstracni priklad pro 1. ukol IPS/2018
 * Ales Smrcka
 */

#include "mmal.h"
#include <sys/mman.h> // mmap
#include <stdbool.h> // bool
#include <assert.h> // assert

#ifdef NDEBUG
/**
 * The structure header encapsulates data of a single memory block.
 *   ---+------+----------------------------+---
 *      |Header|DDD not_free DDDDD...free...|
 *   ---+------+-----------------+----------+---
 *             |-- Header.asize -|
 *             |-- Header.size -------------|
 */
typedef struct header Header;
struct header {

    /**
     * Pointer to the next header. Cyclic list. If there is no other block,
     * points to itself.
     */
    Header *next;

    /// size of the block
    size_t size;

    /**
     * Size of block in bytes allocated for program. asize=0 means the block
     * is not used by a program.
     */
    size_t asize;
};

/**
 * The arena structure.
 *   /--- arena metadata
 *   |     /---- header of the first block
 *   v     v
 *   +-----+------+-----------------------------+
 *   |Arena|Header|.............................|
 *   +-----+------+-----------------------------+
 *
 *   |--------------- Arena.size ---------------|
 */
typedef struct arena Arena;
struct arena {

    /**
     * Pointer to the next arena. Single-linked list.
     */
    Arena *next;

    /// Arena size.
    size_t size;
};

#define PAGE_SIZE (128*1024)

#endif // NDEBUG

Arena *first_arena = NULL;

/**
 * Return size alligned to PAGE_SIZE
 */
static
size_t allign_page(size_t size)
{
    return ((size - 1 + PAGE_SIZE) / PAGE_SIZE) * PAGE_SIZE;;
}

/**
 * Allocate a new arena using mmap.
 * @param req_size requested size in bytes. Should be alligned to PAGE_SIZE.
 * @return pointer to a new arena, if successfull. NULL if error.
 * @pre req_size > sizeof(Arena) + sizeof(Header)
 */

/**
 *   +-----+------------------------------------+
 *   |Arena|....................................|
 *   +-----+------------------------------------+
 *
 *   |--------------- Arena.size ---------------|
 */
static
Arena *arena_alloc(size_t req_size)
{
#ifndef MAP_ANONYMOUS
    #define MAP_ANONYMOUS 0x20
#endif

    //assert(req_size > sizeof(Arena) + sizeof(Header));
    if (req_size <= sizeof(Arena) + sizeof(Header))
        return NULL;

    if (!first_arena) {
        first_arena = mmap(NULL, req_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        first_arena->next = NULL;
        first_arena->size = req_size;
        return first_arena;
    } else {
        Arena *aktual = first_arena;
        while (aktual->next != NULL)
            aktual = aktual->next;

        aktual->next = mmap(aktual, req_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        aktual->next->next = NULL;
        aktual->next->size = req_size;
        return aktual->next;
    }
}

/**
 * Appends a new arena to the end of the arena list.
 * @param a     already allocated arena
 */
/*static
void arena_append(Arena *a)
{
    (void)a;
}*/

/**
 * Header structure constructor (alone, not used block).
 * @param hdr       pointer to block metadata.
 * @param size      size of free block
 * @pre size > 0
 */
/**
 *   +-----+------+------------------------+----+
 *   | ... |Header|........................| ...|
 *   +-----+------+------------------------+----+
 *
 *                |-- Header.size ---------|
 */
static
void hdr_ctor(Header *hdr, size_t size)
{
    //assert(size > 0);

    hdr->next = NULL;
    hdr->size = size;
    hdr->asize = 0;
}

/**
 * Checks if the given free block should be split in two separate blocks.
 * @param hdr       header of the free block
 * @param size      requested size of data
 * @return true if the block should be split
 * @pre hdr->asize == 0
 * @pre size > 0
 */
/*static
bool hdr_should_split(Header *hdr, size_t size)
{
    assert(hdr->asize == 0);
    assert(size > 0);
    // FIXME
    (void)hdr;
    (void)size;
    return false;
}*/

/**
 * Splits one block in two.
 * @param hdr       pointer to header of the big block
 * @param req_size  requested size of data in the (left) block.
 * @return pointer to the new (right) block header.
 * @pre   (hdr->size >= req_size + 2*sizeof(Header))
 */
/**
 * Before:        |---- hdr->size ---------|
 *
 *    -----+------+------------------------+----
 *         |Header|........................|
 *    -----+------+------------------------+----
 *            \----hdr->next---------------^
 */
/**
 * After:         |- req_size -|
 *
 *    -----+------+------------+------+----+----
 *     ... |Header|............|Header|....|
 *    -----+------+------------+------+----+----
 *             \---next--------^  \--next--^
 */
static
Header *hdr_split(Header *hdr, size_t req_size)
{
    //assert((hdr->size >= req_size + 2*sizeof(Header)));

    size_t tsize = hdr->size;
    Header *tnext = hdr->next;
    if (hdr->size < req_size + 2*sizeof(Header))
        return NULL;

    hdr->size = req_size;
    hdr->next = (void*)(((char*)hdr) + req_size + sizeof(Header));

    hdr->next->size = tsize - req_size - sizeof(Header);
    hdr->next->asize = 0;
    hdr->next->next = tnext;

    return hdr->next;
}

/**
 * Detect if two adjacent blocks could be merged.
 * @param left      left block
 * @param right     right block
 * @return true if two block are free and adjacent in the same arena.
 * @pre left->next == right
 * @pre left != right
 */
static
bool hdr_can_merge(Header *left, Header *right)
{
    //assert(left->next == right);
    //assert(left != right);

    size_t vz = (char*)right - (char*)left;
    if (left->size + sizeof(Header) == vz)
        if (right->asize == 0)
            return true;
    return false;
}

/**
 * Merge two adjacent free blocks.
 * @param left      left block
 * @param right     right block
 * @pre left->next == right
 * @pre left != right
 */
static
void hdr_merge(Header *left, Header *right)
{
    //assert(left->next == right);
    //assert(left != right);

    if (hdr_can_merge(left, right)) {
        left->next = right->next;
        left->size += right->size + sizeof(Header);
    }
}

/**
 * Finds the first free block that fits to the requested size.
 * @param size      requested size
 * @return pointer to the header of the block or NULL if no block is available.
 * @pre size > 0
 */
/*static
Header *first_fit(size_t size)
{
    assert(size > 0);
    // FIXME
    (void)size;
    return NULL;
}*/

/**
 * Search the header which is the predecessor to the hdr. Note that if
 * @param hdr       successor of the search header
 * @return pointer to predecessor, hdr if there is just one header.
 * @pre first_arena != NULL
 * @post predecessor->next == hdr
 */
/*static
Header *hdr_get_prev(Header *hdr)
{
    assert(first_arena != NULL);
    (void)hdr;
    return NULL;
}*/

/**
 * Allocate memory. Use first-fit search of available block.
 * @param size      requested size for program
 * @return pointer to allocated data or NULL if error or size = 0.
 */
void *mmalloc(size_t size)
{
    if (!size)
        return NULL;

    Header *first_header;
    if (!first_arena) {
        first_arena = arena_alloc(allign_page(size + sizeof(Arena)));
        if (first_arena == NULL)
            return NULL;
        hdr_ctor((void*)(first_arena + 1), allign_page(size + sizeof(Arena)) - sizeof(Arena) - sizeof(Header));
        first_header = (void*)(first_arena + 1);
        first_header->next = first_header;
    }
    first_header = (void*)(first_arena + 1);
    Header *aktual = first_header;

    //najít volný header
    do {
        if (aktual->asize == 0 && aktual->size >= size) {
            aktual->asize = size;
            hdr_split(aktual, aktual->asize);
            return aktual + 1;
        }
        aktual = aktual->next;
    } while (aktual != first_header);

    //najít header kde by bylo místo
    do {
        if (aktual->size - aktual->asize >= size + sizeof(Header)) {
            aktual = hdr_split(aktual, aktual->asize);
            aktual->asize = size;
            return aktual + 1;
        }
        aktual = aktual->next;
    } while (aktual != first_header);

    //alokovat novou arénu
    Arena *akt = arena_alloc(allign_page(size + sizeof(Arena)));;
    hdr_ctor((void*)(akt + 1), allign_page(size + sizeof(Arena)) - sizeof(Arena) - sizeof(Header));
    Header *second_header = (void*)(akt + 1);
    second_header->next = first_header;
    second_header->asize = size;

    aktual = first_header;
    while (aktual->next != first_header)
        aktual = aktual->next;

    aktual->next = second_header;
    hdr_split(second_header, size);
    return second_header + 1;
}

/**
 * Free memory block.
 * @param ptr       pointer to previously allocated data
 * @pre ptr != NULL
 */
void mfree(void *ptr)
{
    //assert(ptr != NULL);

    if (ptr && first_arena) {
        Header *wanted = ptr - sizeof(Header);
        wanted->asize = 0;

        Header *predchozi = (void*)(first_arena + 1);
        if (predchozi->next != wanted)
            do {
                predchozi = predchozi->next;
                if (predchozi == (void*)(first_arena + 1)) {
                    //fprintf(stderr, "Free: chybny ukazatel");
                    return;
                }
            } while (predchozi->next != wanted);


        Header *dalsi = wanted->next;

        hdr_merge(wanted, dalsi);
        hdr_merge(predchozi, wanted);
    }
}

/**
 * Reallocate previously allocated block.
 * @param ptr       pointer to previously allocated data
 * @param size      a new requested size. Size can be greater, equal, or less
 * then size of previously allocated block.
 * @return pointer to reallocated space or NULL if size equals to 0.
 */
void *mrealloc(void *ptr, size_t size)
{
    if (!size || !ptr || !first_arena)
        return NULL;

    Header *wanted = ptr - sizeof(Header);

        Header *predchozi = (void*)(first_arena + 1);
        if (predchozi->next != wanted)
            do {
                predchozi = predchozi->next;
                if (predchozi == (void*)(first_arena + 1)) {
                    //fprintf(stderr, "Free: chybny ukazatel");
                    return NULL;
                }
            } while (predchozi->next != wanted);


    if (size <= wanted->size) {
        wanted->asize = size;
        return wanted + 1;
    } else {
        hdr_merge(wanted, wanted->next);
        if (size <= wanted->size) {
            wanted->asize = size;
            hdr_split(wanted, size);
            return wanted + 1;
        }
    }
    /////////////////////////////////////
    Header *novy = mmalloc(size) - sizeof(Header);
    if (!novy)
        return NULL;

    for (size_t i = 0; i < wanted->asize; ++i)
        ((char*)novy)[i] = ((char*)wanted)[i];

    mfree(wanted + 1);

    return novy + 1;
}


/*
void *mcalloc(size_t size) {
    char *t = mmalloc(size);
    for(size_t i = 0; i < size; ++i)
        t[i] = 0;
    return t;
}*/
