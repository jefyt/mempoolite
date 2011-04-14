/**
 * The author disclaims copyright to this source code.  In place of
 * a legal notice, here is a blessing:
 *
 *    May you do good and not evil.
 *    May you find forgiveness for yourself and forgive others.
 *    May you share freely, never taking more than you give.
 *
 * This file contains the APIs that implement a memory allocation subsystem
 * based on SQLite's memsys5 memory subsystem. Refer to
 * http://www.sqlite.org/malloc.html for more info.
 *
 * This version of the memory allocation subsystem omits all
 * use of malloc(). The application gives a block of memory
 * from which allocations are made and returned by the mempoolite_malloc()
 * and mempoolite_realloc() implementations.
 *
 * This version of the memory allocation subsystem is included
 * in the build only if MEMPOOLITE_ENABLED is set to 1.
 *
 * This memory allocator uses the following algorithm:
 *
 *   1.  All memory allocations sizes are rounded up to a power of 2.
 *
 *   2.  If two adjacent free blocks are the halves of a larger block,
 *       then the two blocks are coalesed into the single larger block.
 *
 *   3.  New memory is allocated from the first available free block.
 *
 * This algorithm is described in: J. M. Robson. "Bounds for Some Functions
 * Concerning Dynamic Storage Allocation". Journal of the Association for
 * Computing Machinery, Volume 21, Number 8, July 1974, pages 491-499.
 *
 * Let n be the size of the largest allocation divided by the minimum
 * allocation size (after rounding all sizes up to a power of 2.)  Let M
 * be the maximum amount of memory ever outstanding at one time.  Let
 * N be the total amount of memory available for allocation.  Robson
 * proved that this memory allocator will never breakdown due to
 * fragmentation as long as the following constraint holds:
 *
 *      N >=  M*(1 + log2(n)/2) - n + 1
 */
#ifndef MEMPOOLITE_H
#define MEMPOOLITE_H

#include <stdint.h>

/**
 * @brief The function call returns success
 */
#define MEMPOOLITE_OK			0
/**
 * @brief Invalid parameters are passed to a function
 */
#define MEMPOOLITE_ERR_INVPAR	-1
/**
 * @brief Macro to fix unused parameter compiler warning
 */
#define MEMPOOLITE_UNUSED_PARAM(param)	(void)(param)
/**
 * @brief Maximum size of any allocation is ((1 << @ref MEMPOOLITE_LOGMAX) *
 *        mempoolite_t.szAtom). Since mempoolite_t.szAtom is always at least 8 and
 *        32-bit integers are used, it is not actually possible to reach this
 *        limit.
 */
#define MEMPOOLITE_LOGMAX 30
/**
 * @brief Maximum allocation size of this memory pool library. All allocations
 *        must be a power of two and must be expressed by a 32-bit signed
 *        integer. Hence the largest allocation is 0x40000000 or 1073741824.
 */
#define MEMPOOLITE_MAX_ALLOC_SIZE	0x40000000
/**
 * @brief An indicator that a function is a public API
 */
#define MEMPOOLITE_API
/**
 * @brief An indicator that a function is a private API
 */
#define MEMPOOLITE_PRIVATE_API	static

/**
 * @brief Lock object to be used in a threadsafe memory pool
 */
typedef struct mempoolite_lock
{
	void *arg; /**< Argument to be passed to acquire and release function
		pointers */
	int (*acquire)(void *arg); /**< Function pointer to acquire a lock */
	int (*release)(void *arg); /**< Function pointer to release a lock */
} mempoolite_lock_t;

/**
 * @brief Memory pool object
 */
typedef struct mempoolite
{
	/*-------------------------------
	  Memory available for allocation
	  -------------------------------*/
	int szAtom; /**< Smallest possible allocation in bytes */
	int nBlock; /**< Number of szAtom sized blocks in zPool */
	uint8_t *zPool; /**< Memory available to be allocated */

	mempoolite_lock_t lock; /**< Lock to control access to the memory allocation
		subsystem. */

	/*----------------------
	  Performance statistics
	  ----------------------*/
	uint64_t nAlloc; /**< Total number of calls to malloc */
	uint64_t totalAlloc; /**< Total of all malloc calls - includes internal
		fragmentation */
	uint64_t totalExcess; /**< Total internal fragmentation */
	uint32_t currentOut; /**< Current checkout, including internal
		fragmentation */
	uint32_t currentCount; /**< Current number of distinct checkouts */
	uint32_t maxOut; /**< Maximum instantaneous currentOut */
	uint32_t maxCount; /**< Maximum instantaneous currentCount */
	uint32_t maxRequest; /**< Largest allocation (exclusive of internal frag) */

	int aiFreelist[MEMPOOLITE_LOGMAX + 1]; /**< List of free blocks. aiFreelist[0]
		is a list of free blocks of size mempoolite_t.szAtom. aiFreelist[1] holds
		blocks of size szAtom * 2 and so forth.*/

	uint8_t *aCtrl; /**< Space for tracking which blocks are checked out and the
		size of each block.  One byte per block. */
} mempoolite_t;

/**
 * @brief Initialize the memory pool object.
 * @param[in,out] handle Pointer to a @ref mempoolite_t object which is allocated
 *                       by the caller either from stack, heap, or application's
 *                       memory space.
 * @param[in] buf Pointer to a large, contiguous chunk of memory space that
 *                @ref mempoolite_t will use to satisfy all of its memory
 *                allocation needs. This might point to a static array or it
 *                might be memory obtained from some other application-specific
 *                mechanism.
 * @param[in] buf_size The number of bytes of memory space pointed to by @ref
 *                     buf
 * @param[in] min_alloc Minimum size of an allocation. Any call to @ref
 *                      mempoolite_malloc where nBytes is less than min_alloc will
 *                      be rounded up to min_alloc. min_alloc must be a power of
 *                      two.
 * @param[in] lock Pointer to a lock object to control access to the memory
 *                 allocation subsystem of @ref mempoolite_t object. If this is
 *                 @ref NULL, @ref mempoolite_t will be non-threadsafe and can only
 *                 be safely used by a single thread. It is safe to allocate
 *                 this in stack because it will be copied to @ref mempoolite_t
 *                 object.
 * @return @ref MEMPOOLITE_OK on success and @ref MEMPOOLITE_ERR_INVPAR on invalid
 *         parameters error.
 */
MEMPOOLITE_API int mempoolite_init(mempoolite_t *handle, const void *buf,
							 const int buf_size, const int min_alloc,
							 const mempoolite_lock_t *lock);

/**
 * @brief Allocate bytes of memory
 * @param[in,out] handle Pointer to an initialized @ref mempoolite_t object
 * @param[in] nBytes Number of bytes to allocate
 * @return Non-NULL on success, NULL otherwise
 */
MEMPOOLITE_API void *mempoolite_malloc(mempoolite_t *handle, const int nBytes);

/**
 * @brief Free memory
 * @param[in,out] handle Pointer to an initialized @ref mempoolite_t object
 * @param[in] pPrior Allocated buffer
 */
MEMPOOLITE_API void mempoolite_free(mempoolite_t *handle, const void *pPrior);

/**
 * @brief Change the size of an existing memory allocation.
 * @param[in,out] handle Pointer to an initialized @ref mempoolite_t object
 * @param[in] pPrior Existing allocated memory
 * @param[in] nBytes Size of the new memory allocation. This is always a value
 *                   obtained from a prior call to mempoolite_roundup(). Hence,
 *                   this is always a non-negative power of two. If nBytes == 0
 *                   that means that an oversize allocation (an allocation
 *                   larger than @ref MEMPOOLITE_MAX_ALLOC_SIZE) was requested and
 *                   this routine should return NULL without freeing pPrior.
 * @return Non-NULL on success, NULL otherwise
 */
MEMPOOLITE_API void *mempoolite_realloc(mempoolite_t *handle, const void *pPrior,
								  const int nBytes);

/**
 * @brief Round up a request size to the next valid allocation size.
 * @param[in,out] handle Pointer to an initialized @ref mempoolite_t object
 * @param[in] n Request size
 * @return Positive non-zero value if the size can be allocated or zero if the
 *         allocation is too large to be handled.
 */
MEMPOOLITE_API int mempoolite_roundup(mempoolite_t *handle, const int n);

#endif /* #ifndef MEMPOOLITE_H */

