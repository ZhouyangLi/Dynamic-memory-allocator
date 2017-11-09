/*
 * mm.c - The malloc package using an explicit free list.
 *
 * In this approach, there is an explicit free list to keep track of 
 * the free blocks in the heap. Each block has a header and a footer 
 * that contain the block size and status of allocation.
 * Each free block has a prev pointer that points to the previous free,
 * as well as a next pointer points to the next free block. It first gives
 * the entire free space on heap to the free list. When it allocates a
 * block to the heap, it will search for the first free block in the
 * free block list that is large enough to hold the size and allocate
 * the desired size. If there is still usable space that remains in the allocated
 * free block, it will be cut out and put back to the free list, and then
 * coalesced with adjacent free blocks. If there is no enough space in the
 * heap to allocate, then the heap will expand, and the remaining free space
 * in the old heap will coalesce with the additional heap space. Everytime it
 * frees a block, the block will coalesce with its adjacent free blocks and
 * then be added to the free list. Realloc is implemented directly using 
 * mm_malloc and mm_free.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
   /* Team name */
    "foo(ls)",
    /* First member's full name */
    "Shirley Yang",
    /* First member's github username*/
    "syang26",
    /* Second member's full name (leave blank if none) */
    "Zhouyang Li",
    /* Second member's github username (leave blank if none) */
    "ZhouyangLi",
    /*Third member's full name */
    "Yuxin Lu",
    /*Third member's github username*/
    "Cindyluyx"

};

/* Basic constants and macros */
#define WSIZE       4       /* Word and header/footer size (bytes) */
#define DSIZE       8       /* Double word size (bytes) */
#define ALIGNSIZE   16      /* Alignment size (byte) */
#define CHUNKSIZE   (1<<12) /* Extend heap by this amount (bytes) */
#define HSZ         sizeof(hdr_t)   /*size of free header */
#define START       mem_heap_lo ()
#define END         mem_heap_hi ()

#define MAX(x,y)    ((x) > (y)? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)   ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)      (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)     (GET(p) & ~0x7)
#define GET_ALLOC(p)    (GET(p) & 0x1)

/* Given header ptr hp, compute address of its header and footer */
#define HDRP(hp)        (char *)(hp)
#define FTRP(hp)        ((char *)(hp) + GET_SIZE(HDRP(hp)) - WSIZE)

/* Given header ptr hp, compute address of next and previous blocks */
#define NEXT_BLKP(hp)   ((char *)(hp) + GET_SIZE(HDRP(hp)))
#define PREV_BLKP(hp)   ((char *)(hp) - GET_SIZE(((char *)(hp) - WSIZE)))

static char *heap_listp;

/*
* defining a new struct for header
*/

typedef struct header {
    unsigned int size_status; // packed infomation of block size and status (0 is free, 1 is allocated)
    struct header *next;
    struct header *prev;
} hdr_t;


static hdr_t *flist; // pointer to the first free block



/*
* The Linked List Methods
*/
void list_insert (hdr_t ** head, hdr_t *n)
{
    //if there is no head, make n the head
    if (*head == NULL)
    {
        n -> prev = NULL;
        n -> next = NULL;
        *head = n;
    }

    else //insert n at the front of the list
    {
        n -> next = *head;
        n -> prev = NULL;
        (*head) -> prev = n;
        *head = n;

    }

}


void list_remove(hdr_t **head, hdr_t *n) {
    if (*head == NULL) {
        printf("Error: list_remove: No such list exist!\n");

    }
    else if(*head == n) {
        //remove head
        if ((*head) -> next == NULL) {
            *head = NULL;
        }
        else {
            ((*head) -> next) -> prev = NULL;
            *head = (*head) -> next;
        }
        return;
    }
    else {
        hdr_t *current = *head;

        while(current -> next != NULL) {
            current = current -> next;
            if(current==n) {
                //remove current
                (current -> prev) -> next = current -> next;
                if (current -> next != NULL) {
                    (current -> next) -> prev = current -> prev;
                }
                return;   
            }
        }
        printf("Error: list_remove: hdr_t n not found!\n");
    }
}


static void *coalesce(void *bp) 
{

    size_t prev_alloc = GET_ALLOC((char*)(bp) - WSIZE);

    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if(prev_alloc && next_alloc) {  /* case 1 */
        list_insert(&flist, (hdr_t*)bp);
        return bp;
    }
    else if(prev_alloc && !next_alloc) {    /* case 2 */
        hdr_t *nextp = (hdr_t*)HDRP(NEXT_BLKP(bp));
        list_remove(&flist, nextp);

        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));

        list_insert(&flist, (hdr_t*)bp);
    }
    else if(!prev_alloc && next_alloc) {
        hdr_t *prevp = (hdr_t*)HDRP(PREV_BLKP(bp));
        list_remove(&flist, prevp);

        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)),PACK(size, 0));
        bp = PREV_BLKP(bp);

        list_insert(&flist, (hdr_t*)bp);
    }
    else {
        hdr_t *prevp = (hdr_t*)HDRP(PREV_BLKP(bp));
        list_remove(&flist, prevp);
        hdr_t *nextp = (hdr_t*)HDRP(NEXT_BLKP(bp));
        list_remove(&flist, nextp);

        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);

        list_insert(&flist, (hdr_t*)bp);
    }
    return bp;
}


/* extends the heap with a new free block */

static void *extend_heap(size_t words) {
    char *ptr;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words +1) * WSIZE : words * WSIZE;
    if ((long)(ptr = mem_sbrk(size)) == -1)
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    void* hp = ((char*)(ptr) - WSIZE);
    //list_insert(&flist, (hdr_t*)hp);             /* Add the free block to the first of free list*/
    PUT(HDRP(hp), PACK(size, 0));                /* Free block header */
    PUT(FTRP(hp), PACK(size, 0));                /* Free block footer */
    PUT(FTRP(hp) + WSIZE, PACK(0, 1));           /* New epilogue header */

    /* Coalesce if the previous block was free */
    return coalesce(hp);
}



/* 
 * mm_init - initialize the malloc package.
 */
//returns -1 if theres a problem, 0 if its ok 
int mm_init(void)
{
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);                             /* Alignment padding */
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));    /* Prologue header */    
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));    /* Prolouue footer */
    PUT(heap_listp + (3*WSIZE), PACK(0,1));         /* Epilogue header */
    heap_listp += (2*WSIZE);

    flist = NULL;
    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
   
    return 0;
}


static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));
    list_remove(&flist, (hdr_t*)bp);

    if((csize - asize) >= (HSZ + WSIZE)) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));
        list_insert(&flist, (hdr_t*)bp);
    }
    else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }

}


static void *find_fit(size_t asize)
{
    /* First-fit search*/
    hdr_t *hp;
    for(hp = flist; hp != NULL; hp = hp -> next) {
        size_t size = GET_SIZE(HDRP(hp));
        if(size >= asize) {
            return (void*) hp;
        }
    }
    return NULL; /* No fit */
//#endif
}


/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;   /* Adjusted block size */
    size_t extendsize;  /* Amount to extend heap if no fit */
    char *bp;

    /* Ignore spurious requests */
    if(size == 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs */
    if(size <= DSIZE)
        asize = ALIGNSIZE;
    else
        asize = ALIGNSIZE * ((size + (DSIZE) + (ALIGNSIZE-1)) / ALIGNSIZE);

    /* Search the free list for fit */
    if((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        bp = (char*)(bp) + WSIZE;
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    bp = (char*)(bp) + WSIZE;
    return bp;
}


/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    ptr = (char*)(ptr) - WSIZE;

    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
}


/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    if(ptr == NULL)
        return mm_malloc(size);
    if(size == 0) {
        mm_free(ptr);
        return NULL;
    }
       
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = *(size_t *)((char *)oldptr - WSIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

char *found (char* h, unsigned int s, char * check [5000])
{
    int f = 0;
    char * existsalready;
    char * ret;
    while (h != NULL)
    {
        if (GET_SIZE (h) == s)
        {
            for (int i = 0; i < 5000; i++)
            {
                if (h == check [i])
                    existsalready = h;
            }

            if (!existsalready) //if not in check
            {
                f = 1;
                ret =h;
            }
            else
                return NULL;
        }
        h = h + GET_SIZE (h);
    }

    if (f == 1) return ret;
    return NULL;
}

void add (char * ad [5000], char * toadd)
{
    for (int i = 0; i < 5000; i++)
    {
        if (ad [i] == NULL)
        {
            ad[i] = toadd;
            return;
        }
    }
}

void mm_checkheap(int verbose) 
{
    int error = 0;
    if (verbose != 0)//if checkheap is turned on
    {
        //check if every block in list is free
        printf ("Checking flist \n");
        hdr_t *cur = flist;
        while (cur != NULL)
        {
            if (GET_ALLOC(cur) != 0)
            {
                printf ("ERROR: block in flist not free \n");
                error = 1;
            }
            cur = cur -> next;
        }

        //check if check if coalesce works
        printf ("Checking coalesce \n");
        hdr_t * nblock = HDRP (heap_listp);
        while (nblock != NULL)
        {
            if (GET_ALLOC(nblock) == 0)
            {
                if (GET_ALLOC (nblock -> next) == 0)
                {
                    printf ("ERROR: adjacent free blocks in heap \n");
                    error = 1;
                }
            }
            nblock = nblock + GET_SIZE (nblock);
        }

        //check if every free block is in flist
        printf ("Checking if free blocks are in flist");
        char * nb = heap_listp;
        char * checkflist [5000] = {0}; // array of addresses of sizes that have already been checked
        while (nb != NULL) //loop through heap
        {
            if (GET_ALLOC(nb) == 0) //if its free
            {
                char * n = (char*) flist; // loop through freelist
                while (n != NULL)
                {
                    //if the size is found in free list and not also in check, add to check
                    //if the size if not found or if it is in check, error
                    if (found (n, GET_SIZE(nb), checkflist))
                    {
                        add (checkflist, nb);
                    }

                    else
                    {
                        printf("ERROR: Not every free block is in flist\n" );
                        error = 1;
                    }
                    n = n + GET_SIZE (n);
                }
            }
            nb = nb + GET_SIZE (nb);
        }

        //check if pointers in free list point to valid free blocks
        printf ("Checking if flist has invalid pointers");
        hdr_t *in = flist;
        hdr_t *ip = flist -> next;
        while (in != NULL || ip != NULL)
        {
            if ((in < (hdr_t *)START || in > (hdr_t*)END) && in)
            {
                printf("ERROR: flist has invalid next pointers\n");
                error = 1;
            }
            in = in -> next;

            if (((ip -> prev) < (hdr_t *) START && (ip-> prev) > (hdr_t *) END) && ip)
            {
                printf("ERROR: flist has invalid prev pointers\n");
                error = 1;
            }
            ip = ip -> next;
        }

        //check if allocated blocks overlap //if hdr +size != next block
        printf("Checking if there are overlap\n");
        char * nb2 = heap_listp;
        char * b1 = nb2;
        char * b2 = b1 + GET_SIZE(b1);
        while (b1 != NULL)
        {
            if ((b1 + GET_SIZE(b1)) != HDRP (b2))
            {
                printf ("ERROR: block pointers overlap\n");
                error = 1;
            }
            b2 = b1;
            b1 = b1 + GET_SIZE (b1);
        }

    }

    if (!error)
    {
        printf("CONGRATULATIONS! mm_malloc passes check_heap tests");
    }

    else
    {
        printf ("CheckHeap is turned off");
    }
    return;
}

