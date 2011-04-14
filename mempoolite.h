#ifndef MEMPOOLITE_H
#define MEMPOOLITE_H

#include <stdint.h>

typedef uint8_t u8;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int64_t s64;

/*
** Maximum size of any allocation is ((1<<MEMPOOLITE_LOGMAX)*mempoolite_t.szAtom). Since
** mempoolite_t.szAtom is always at least 8 and 32-bit integers are used,
** it is not actually possible to reach this limit.
*/
#define MEMPOOLITE_LOGMAX 30

typedef struct mempoolite_mutex
{
	void *mutex;
	int (*lock)(void *mutex);
	int (*unlock)(void *mutex);
} mempoolite_mutex_t;

typedef struct mempoolite {
	/*
	** Memory available for allocation
	*/
	int szAtom;      /* Smallest possible allocation in bytes */
	int nBlock;      /* Number of szAtom sized blocks in zPool */
	u8 *zPool;       /* Memory available to be allocated */

	/*
	** Mutex to control access to the memory allocation subsystem.
	*/
	mempoolite_mutex_t mutex;

	/*
	** Performance statistics
	*/
	u64 nAlloc;         /* Total number of calls to malloc */
	u64 totalAlloc;     /* Total of all malloc calls - includes internal frag */
	u64 totalExcess;    /* Total internal fragmentation */
	u32 currentOut;     /* Current checkout, including internal fragmentation */
	u32 currentCount;   /* Current number of distinct checkouts */
	u32 maxOut;         /* Maximum instantaneous currentOut */
	u32 maxCount;       /* Maximum instantaneous currentCount */
	u32 maxRequest;     /* Largest allocation (exclusive of internal frag) */

	/*
	** Lists of free blocks.  aiFreelist[0] is a list of free blocks of
	** size mempoolite_t.szAtom.  aiFreelist[1] holds blocks of size szAtom*2.
	** and so forth.
	*/
	int aiFreelist[MEMPOOLITE_LOGMAX+1];

	/*
	** Space for tracking which blocks are checked out and the size
	** of each block.  One byte per block.
	*/
	u8 *aCtrl;
} mempoolite_t;

int mempoolite_construct(mempoolite_t *handle, void *buf, const int buf_size, const int min_alloc, const mempoolite_mutex_t *mutex);
void mempoolite_destruct(mempoolite_t *handle);
void *mempoolite_malloc(mempoolite_t *handle, const int nBytes);
void mempoolite_free(mempoolite_t *handle, void *pPrior);
void *mempoolite_realloc(mempoolite_t *handle, void *pPrior, const int nBytes);
int mempoolite_roundup(mempoolite_t *handle, const int n);

#endif /* #ifndef MEMPOOLITE_H */

