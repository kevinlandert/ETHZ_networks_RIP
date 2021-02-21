/* Filename: rmutex.c */

#include <assert.h>
#include <pthread.h>
#include "rmutex.h"


void rmutex_init( rmutex_t* lock ) {
    lock->lock_depth = 0;

    pthread_cond_init( &lock->control_cv, NULL );
    pthread_mutex_init( &lock->control_mutex, NULL );
}

void rmutex_lock( rmutex_t* lock ) {
    pthread_mutex_lock( &lock->control_mutex );

    /* wait until the recursive lock is unlocked by its owner */
    while( lock->lock_depth > 0 && lock->owner != pthread_self() )
        pthread_cond_wait( &lock->control_cv, &lock->control_mutex );

    if( lock->lock_depth == 0 ) { /* lock was free */
        lock->owner = pthread_self();
        lock->lock_depth = 1;
    }
    else if( lock->owner == pthread_self() ) /* recursively locking it */
        lock->lock_depth += 1;

    pthread_mutex_unlock( &lock->control_mutex );
}

void rmutex_unlock( rmutex_t* lock ) {
    pthread_mutex_lock( &lock->control_mutex );

    assert( lock->owner == pthread_self() );
    assert( lock->lock_depth > 0 );

    lock->lock_depth -= 1;

    /* wake up those waiting on the lock if we completely released it */
    if( lock->lock_depth == 0 )
        pthread_cond_signal( &lock->control_cv );

    pthread_mutex_unlock( &lock->control_mutex );
}

void rmutex_destroy( rmutex_t* lock ) {
    pthread_cond_destroy( &lock->control_cv );
    pthread_mutex_destroy( &lock->control_mutex );
}
