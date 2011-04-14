/*
** 2007 October 14
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** This file contains the C functions that implement a memory
** allocation subsystem based on SQLite's memsys5 memory subsystem.
**
** This version of the memory allocation subsystem omits all
** use of malloc(). The application gives a block of memory
** from which allocations are made and returned by the mempoolite_malloc()
** and mempoolite_realloc() implementations.
**
** This version of the memory allocation subsystem is included
** in the build only if MEMPOOLITE_ENABLED is set to 1.
**
** This memory allocator uses the following algorithm:
**
**   1.  All memory allocations sizes are rounded up to a power of 2.
**
**   2.  If two adjacent free blocks are the halves of a larger block,
**       then the two blocks are coalesed into the single larger block.
**
**   3.  New memory is allocated from the first available free block.
**
** This algorithm is described in: J. M. Robson. "Bounds for Some Functions
** Concerning Dynamic Storage Allocation". Journal of the Association for
** Computing Machinery, Volume 21, Number 8, July 1974, pages 491-499.
**
** Let n be the size of the largest allocation divided by the minimum
** allocation size (after rounding all sizes up to a power of 2.)  Let M
** be the maximum amount of memory ever outstanding at one time.  Let
** N be the total amount of memory available for allocation.  Robson
** proved that this memory allocator will never breakdown due to
** fragmentation as long as the following constraint holds:
**
**      N >=  M*(1 + log2(n)/2) - n + 1
**
** The sqlite3_status() logic tracks the maximum values of n and M so
** that an application can, at any time, verify this constraint.
*/
#include "mempoolite.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

/*
** This version of the memory allocator is used only when
** MEMPOOLITE_ENABLED is set to 1.
*/
#if MEMPOOLITE_ENABLED

/*
** A minimum allocation is an instance of the following structure.
** Larger allocations are an array of these structures where the
** size of the array is a power of 2.
**
** The size of this object must be a power of two.  That fact is
** verified in mempoolite_construct().
*/
typedef struct mempoolite_link mempoolite_link_t;
struct mempoolite_link {
	int next;       /* Index of next free chunk */
	int prev;       /* Index of previous free chunk */
};

/*
** Masks used for mempoolite_t.aCtrl[] elements.
*/
#define MEMPOOLITE_CTRL_LOGSIZE  0x1f    /* Log2 Size of this block */
#define MEMPOOLITE_CTRL_FREE     0x20    /* True if not checked out */

/*
** Assuming mempoolite_t.zPool is divided up into an array of mempoolite_link_t
** structures, return a pointer to the idx-th such lik.
*/
#define mempoolite_getlink(handle, idx) ((mempoolite_link_t *)(&handle->zPool[(idx)*handle->szAtom]))

#define mempoolite_enter(handle)			if((handle != NULL) && ((handle)->mutex.mutex != NULL))	\
										{ (handle)->mutex.lock((handle)->mutex.mutex); }
#define mempoolite_leave(handle)			if((handle != NULL) && ((handle)->mutex.mutex != NULL))	\
										{ (handle)->mutex.unlock((handle)->mutex.mutex); }

static int mempoolite_logarithm(int iValue);
static int mempoolite_size(const mempoolite_t *handle, void *p);
static void mempoolite_link(mempoolite_t *handle, int i, int iLogsize);
static void mempoolite_unlink(mempoolite_t *handle, int i, int iLogsize);
static int mempoolite_unlink_first(mempoolite_t *handle, int iLogsize);
static void *mempoolite_malloc_unsafe(mempoolite_t *handle, int nByte);
static void mempoolite_free_unsafe(mempoolite_t *handle, void *pOld);

int mempoolite_construct(mempoolite_t *handle, void *buf, const int buf_size, const int min_alloc, const mempoolite_mutex_t *mutex)
{
	int ii;            /* Loop counter */
	int nByte;         /* Number of bytes of memory available to this allocator */
	u8 *zByte;         /* Memory usable by this allocator */
	int nMinLog;       /* Log base 2 of minimum allocation size in bytes */
	int iOffset;       /* An offset into handle->aCtrl[] */

	/* Copy the mutex if it is not NULL */
	if(mutex != NULL) {
		memcpy(&handle->mutex, mutex, sizeof(handle->mutex));
	}
	else {
		handle->mutex.mutex = NULL;
	}

	/* The size of a mempoolite_link_t object must be a power of two.  Verify that
	** this is case.
	*/
	assert( (sizeof(mempoolite_link_t)&(sizeof(mempoolite_link_t)-1))==0 );

	nByte = buf_size;
	zByte = (u8*)buf;
	assert( zByte!=0 );

	nMinLog = mempoolite_logarithm(min_alloc);
	handle->szAtom = (1<<nMinLog);
	while( (int)sizeof(mempoolite_link_t)>handle->szAtom ) {
		handle->szAtom = handle->szAtom << 1;
	}

	handle->nBlock = (nByte / (handle->szAtom+sizeof(u8)));
	handle->zPool = zByte;
	handle->aCtrl = (u8 *)&handle->zPool[handle->nBlock*handle->szAtom];

	for(ii=0; ii<=MEMPOOLITE_LOGMAX; ii++) {
		handle->aiFreelist[ii] = -1;
	}

	iOffset = 0;
	for(ii=MEMPOOLITE_LOGMAX; ii>=0; ii--) {
		int nAlloc = (1<<ii);
		if( (iOffset+nAlloc)<=handle->nBlock ) {
			handle->aCtrl[iOffset] = ii | MEMPOOLITE_CTRL_FREE;
			mempoolite_link(handle, iOffset, ii);
			iOffset += nAlloc;
		}
		assert((iOffset+nAlloc)>handle->nBlock);
	}

	return 0;
}

void mempoolite_destruct(mempoolite_t *handle)
{
	/* For now, do nothing */
	handle = handle;
	return;
}

void *mempoolite_malloc(mempoolite_t *handle, const int nBytes) {
	s64 *p = 0;
	if( nBytes>0 ) {
		mempoolite_enter(handle);
		p = mempoolite_malloc_unsafe(handle, nBytes);
		mempoolite_leave(handle);
	}
	return (void*)p;
}

void mempoolite_free(mempoolite_t *handle, void *pPrior) {
	assert( pPrior!=0 );
	mempoolite_enter(handle);
	mempoolite_free_unsafe(handle, pPrior);
	mempoolite_leave(handle);
}

void *mempoolite_realloc(mempoolite_t *handle, void *pPrior, const int nBytes) {
	int nOld;
	void *p;
	assert( pPrior!=0 );
	assert( (nBytes&(nBytes-1))==0 );  /* EV: R-46199-30249 */
	assert( nBytes>=0 );
	if( nBytes==0 ) {
		return 0;
	}
	nOld = mempoolite_size(handle, pPrior);
	if( nBytes<=nOld ) {
		return pPrior;
	}
	mempoolite_enter(handle);
	p = mempoolite_malloc_unsafe(handle, nBytes);
	if( p ) {
		memcpy(p, pPrior, nOld);
		mempoolite_free_unsafe(handle, pPrior);
	}
	mempoolite_leave(handle);
	return p;
}

int mempoolite_roundup(mempoolite_t *handle, const int n) {
	int iFullSz;
	if( n > 0x40000000 ) return 0;
	for(iFullSz=handle->szAtom; iFullSz<n; iFullSz *= 2);
	return iFullSz;
}

/*
** Return the ceiling of the logarithm base 2 of iValue.
**
** Examples:   mempoolite_logarithm(1) -> 0
**             mempoolite_logarithm(2) -> 1
**             mempoolite_logarithm(4) -> 2
**             mempoolite_logarithm(5) -> 3
**             mempoolite_logarithm(8) -> 3
**             mempoolite_logarithm(9) -> 4
*/
static int mempoolite_logarithm(int iValue) {
	int iLog;
	for(iLog=0; (1<<iLog)<iValue; iLog++);
	return iLog;
}

/*
** Return the size of an outstanding allocation, in bytes.  The
** size returned omits the 8-byte header overhead.  This only
** works for chunks that are currently checked out.
*/
static int mempoolite_size(const mempoolite_t *handle, void *p) {
	int iSize = 0;
	if( p ) {
		int i = ((u8 *)p-handle->zPool)/handle->szAtom;
		assert( i>=0 && i<handle->nBlock );
		iSize = handle->szAtom * (1 << (handle->aCtrl[i]&MEMPOOLITE_CTRL_LOGSIZE));
	}
	return iSize;
}

/*
** Link the chunk at handle->aPool[i] so that is on the iLogsize
** free list.
*/
static void mempoolite_link(mempoolite_t *handle, int i, int iLogsize) {
	int x;
	assert( i>=0 && i<handle->nBlock );
	assert( iLogsize>=0 && iLogsize<=MEMPOOLITE_LOGMAX );
	assert( (handle->aCtrl[i] & MEMPOOLITE_CTRL_LOGSIZE)==iLogsize );

	x = mempoolite_getlink(handle, i)->next = handle->aiFreelist[iLogsize];
	mempoolite_getlink(handle, i)->prev = -1;
	if( x>=0 ) {
		assert( x<handle->nBlock );
		mempoolite_getlink(handle, x)->prev = i;
	}
	handle->aiFreelist[iLogsize] = i;
}

/*
** Unlink the chunk at handle->aPool[i] from list it is currently
** on.  It should be found on handle->aiFreelist[iLogsize].
*/
static void mempoolite_unlink(mempoolite_t *handle, int i, int iLogsize) {
	int next, prev;
	assert( i>=0 && i<handle->nBlock );
	assert( iLogsize>=0 && iLogsize<=MEMPOOLITE_LOGMAX );
	assert( (handle->aCtrl[i] & MEMPOOLITE_CTRL_LOGSIZE)==iLogsize );

	next = mempoolite_getlink(handle, i)->next;
	prev = mempoolite_getlink(handle, i)->prev;
	if( prev<0 ) {
		handle->aiFreelist[iLogsize] = next;
	} else {
		mempoolite_getlink(handle, prev)->next = next;
	}
	if( next>=0 ) {
		mempoolite_getlink(handle, next)->prev = prev;
	}
}

/*
** Find the first entry on the freelist iLogsize.  Unlink that
** entry and return its index.
*/
static int mempoolite_unlink_first(mempoolite_t *handle, int iLogsize) {
	int i;
	int iFirst;

	assert( iLogsize>=0 && iLogsize<=MEMPOOLITE_LOGMAX );
	i = iFirst = handle->aiFreelist[iLogsize];
	assert( iFirst>=0 );
	while( i>0 ) {
		if( i<iFirst ) iFirst = i;
		i = mempoolite_getlink(handle, i)->next;
	}
	mempoolite_unlink(handle, iFirst, iLogsize);
	return iFirst;
}

/*
** Return a block of memory of at least nBytes in size.
** Return NULL if unable.  Return NULL if nBytes==0.
**
** The caller guarantees that nByte positive.
**
** The caller has obtained a mutex prior to invoking this
** routine so there is never any chance that two or more
** threads can be in this routine at the same time.
*/
static void *mempoolite_malloc_unsafe(mempoolite_t *handle, int nByte) {
	int i;           /* Index of a handle->aPool[] slot */
	int iBin;        /* Index into handle->aiFreelist[] */
	int iFullSz;     /* Size of allocation rounded up to power of 2 */
	int iLogsize;    /* Log2 of iFullSz/POW2_MIN */

	/* nByte must be a positive */
	assert( nByte>0 );

	/* Keep track of the maximum allocation request.  Even unfulfilled
	** requests are counted */
	if( (u32)nByte>handle->maxRequest ) {
		handle->maxRequest = nByte;
	}

	/* Abort if the requested allocation size is larger than the largest
	** power of two that we can represent using 32-bit signed integers.
	*/
	if( nByte > 0x40000000 ) {
		return 0;
	}

	/* Round nByte up to the next valid power of two */
	for(iFullSz=handle->szAtom, iLogsize=0; iFullSz<nByte; iFullSz *= 2, iLogsize++) {}

	/* Make sure handle->aiFreelist[iLogsize] contains at least one free
	** block.  If not, then split a block of the next larger power of
	** two in order to create a new free block of size iLogsize.
	*/
	for(iBin=iLogsize; handle->aiFreelist[iBin]<0 && iBin<=MEMPOOLITE_LOGMAX; iBin++) {}
	if( iBin>MEMPOOLITE_LOGMAX ) {
		return NULL;
	}
	i = mempoolite_unlink_first(handle, iBin);
	while( iBin>iLogsize ) {
		int newSize;

		iBin--;
		newSize = 1 << iBin;
		handle->aCtrl[i+newSize] = MEMPOOLITE_CTRL_FREE | iBin;
		mempoolite_link(handle, i+newSize, iBin);
	}
	handle->aCtrl[i] = iLogsize;

	/* Update allocator performance statistics. */
	handle->nAlloc++;
	handle->totalAlloc += iFullSz;
	handle->totalExcess += iFullSz - nByte;
	handle->currentCount++;
	handle->currentOut += iFullSz;
	if( handle->maxCount<handle->currentCount ) handle->maxCount = handle->currentCount;
	if( handle->maxOut<handle->currentOut ) handle->maxOut = handle->currentOut;

	/* Return a pointer to the allocated memory. */
	return (void*)&handle->zPool[i*handle->szAtom];
}

/*
** Free an outstanding memory allocation.
*/
static void mempoolite_free_unsafe(mempoolite_t *handle, void *pOld) {
	u32 size, iLogsize;
	int iBlock;

	/* Set iBlock to the index of the block pointed to by pOld in
	** the array of handle->szAtom byte blocks pointed to by handle->zPool.
	*/
	iBlock = ((u8 *)pOld-handle->zPool)/handle->szAtom;

	/* Check that the pointer pOld points to a valid, non-free block. */
	assert( iBlock>=0 && iBlock<handle->nBlock );
	assert( ((u8 *)pOld-handle->zPool)%handle->szAtom==0 );
	assert( (handle->aCtrl[iBlock] & MEMPOOLITE_CTRL_FREE)==0 );

	iLogsize = handle->aCtrl[iBlock] & MEMPOOLITE_CTRL_LOGSIZE;
	size = 1<<iLogsize;
	assert( iBlock+size-1<(u32)handle->nBlock );

	handle->aCtrl[iBlock] |= MEMPOOLITE_CTRL_FREE;
	handle->aCtrl[iBlock+size-1] |= MEMPOOLITE_CTRL_FREE;
	assert( handle->currentCount>0 );
	assert( handle->currentOut>=(size*handle->szAtom) );
	handle->currentCount--;
	handle->currentOut -= size*handle->szAtom;
	assert( handle->currentOut>0 || handle->currentCount==0 );
	assert( handle->currentCount>0 || handle->currentOut==0 );

	handle->aCtrl[iBlock] = MEMPOOLITE_CTRL_FREE | iLogsize;
	while( iLogsize<MEMPOOLITE_LOGMAX ) {
		int iBuddy;
		if( (iBlock>>iLogsize) & 1 ) {
			iBuddy = iBlock - size;
		} else {
			iBuddy = iBlock + size;
		}
		assert( iBuddy>=0 );
		if( (iBuddy+(1<<iLogsize))>handle->nBlock ) break;
		if( handle->aCtrl[iBuddy]!=(MEMPOOLITE_CTRL_FREE | iLogsize) ) break;
		mempoolite_unlink(handle, iBuddy, iLogsize);
		iLogsize++;
		if( iBuddy<iBlock ) {
			handle->aCtrl[iBuddy] = MEMPOOLITE_CTRL_FREE | iLogsize;
			handle->aCtrl[iBlock] = 0;
			iBlock = iBuddy;
		} else {
			handle->aCtrl[iBlock] = MEMPOOLITE_CTRL_FREE | iLogsize;
			handle->aCtrl[iBuddy] = 0;
		}
		size *= 2;
	}
	mempoolite_link(handle, iBlock, iLogsize);
}

#endif /* #if MEMPOOLITE_ENABLED */

