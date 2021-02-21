/*
 * File: rmutex.h
 * Purpose: build a recursive lock out of pthread_mutex_t (support for
 *          recursive mutex locks is sparse in general).
 */

#ifndef _RMUTEX_H_
#define _RMUTEX_H_

/* recursive mutex data type */
typedef struct {
    int lock_depth;          /* 0 means not locked or owned   */
    pthread_t owner;         /* thread id of the owner        */

    /* objects to synchronize methods which operate on rmutex */
    pthread_cond_t  control_cv;
    pthread_mutex_t control_mutex;
} rmutex_t;

/** Initializes the rmutex. */
void rmutex_init( rmutex_t* lock );

/**
 * Locks the lock.  May be done recursively by the same thread (but it needs to
 * call rmutex_unlock an equal number of times to release the lock).
 */
void rmutex_lock( rmutex_t* lock );

/**
 * Unlocks the lock.  If it was locked multiple times, this unlocks only one of
 * the recursive locks.
 */
void rmutex_unlock( rmutex_t* lock );

/** Desroys the lock. */
void rmutex_destroy( rmutex_t* lock );

#endif /* _RMUTEX_H_ */
