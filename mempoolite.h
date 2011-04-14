#ifndef MEMPOOLITE_H
#define MEMPOOLITE_H

#include <stdint.h>

#define MEMPOOLITE_OK			0
#define MEMPOOLITE_ERR_INVPAR	-1
#define MEMPOOLITE_ERR_NOMEM	-2

#define MEMPOOLITE_UNUSED_PARAM(param)	(void)(param)

/*
 ** Maximum size of any allocation is ((1<<MEMPOOLITE_LOGMAX)*mempoolite_t.szAtom). Since
 ** mempoolite_t.szAtom is always at least 8 and 32-bit integers are used,
 ** it is not actually possible to reach this limit.
 */
#define MEMPOOLITE_LOGMAX 30

typedef struct mempoolite_lock
{
	void *arg;
	int (*acquire)(void *arg);
	int (*release)(void *arg);
} mempoolite_lock_t;

typedef struct mempoolite
{
	/*
	 ** Memory available for allocation
	 */
	int szAtom; /* Smallest possible allocation in bytes */
	int nBlock; /* Number of szAtom sized blocks in zPool */
	uint8_t *zPool; /* Memory available to be allocated */

	/*
	 ** Lock to control access to the memory allocation subsystem.
	 */
	mempoolite_lock_t lock;

	/*
	 ** Performance statistics
	 */
	uint64_t nAlloc; /* Total number of calls to malloc */
	uint64_t totalAlloc; /* Total of all malloc calls - includes internal frag */
	uint64_t totalExcess; /* Total internal fragmentation */
	uint32_t currentOut; /* Current checkout, including internal fragmentation */
	uint32_t currentCount; /* Current number of distinct checkouts */
	uint32_t maxOut; /* Maximum instantaneous currentOut */
	uint32_t maxCount; /* Maximum instantaneous currentCount */
	uint32_t maxRequest; /* Largest allocation (exclusive of internal frag) */

	/*
	 ** Lists of free blocks.  aiFreelist[0] is a list of free blocks of
	 ** size mempoolite_t.szAtom.  aiFreelist[1] holds blocks of size szAtom*2.
	 ** and so forth.
	 */
	int aiFreelist[MEMPOOLITE_LOGMAX + 1];

	/*
	 ** Space for tracking which blocks are checked out and the size
	 ** of each block.  One byte per block.
	 */
	uint8_t *aCtrl;
} mempoolite_t;

/*
 ** Initialize the memory allocator.
 **
 ** This routine is not threadsafe.  The caller must be holding a lock
 ** to prevent multiple threads from entering at the same time.
 */
int mempoolite_init(mempoolite_t *handle, const void *buf, const int buf_size,
				 const int min_alloc, const mempoolite_lock_t *lock);

/*
 ** Allocate nBytes of memory
 */
void *mempoolite_malloc(mempoolite_t *handle, const int nBytes);

/*
 ** Free memory.
 **
 ** The outer layer memory allocator prevents this routine from
 ** being called with pPrior==0.
 */
void mempoolite_free(mempoolite_t *handle, const void *pPrior);

/*
 ** Change the size of an existing memory allocation.
 **
 ** The outer layer memory allocator prevents this routine from
 ** being called with pPrior==0.
 **
 ** nBytes is always a value obtained from a prior call to
 ** memsys5Round().  Hence nBytes is always a non-negative power
 ** of two.  If nBytes==0 that means that an oversize allocation
 ** (an allocation larger than 0x40000000) was requested and this
 ** routine should return 0 without freeing pPrior.
 */
void *mempoolite_realloc(mempoolite_t *handle, const void *pPrior, const int nBytes);

/*
 ** Round up a request size to the next valid allocation size.  If
 ** the allocation is too large to be handled by this allocation system,
 ** return 0.
 **
 ** All allocations must be a power of two and must be expressed by a
 ** 32-bit signed integer.  Hence the largest allocation is 0x40000000
 ** or 1073741824 bytes.
 */
int mempoolite_roundup(mempoolite_t *handle, const int n);

#endif /* #ifndef MEMPOOLITE_H */

