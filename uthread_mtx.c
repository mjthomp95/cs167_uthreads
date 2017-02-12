/*
 *   FILE: uthread_mtx.c
 * AUTHOR: Peter Demoreuille
 *  DESCR: userland mutexes
 *   DATE: Sat Sep  8 12:40:00 2001
 *
 *
 * Modified to handle time slicing by Tom Doeppner
 *   DATE: Sun Jan 10, 2016
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "list.h"
#include "uthread.h"
#include "uthread_mtx.h"
#include "uthread_sched.h"



/*
 * uthread_mtx_init
 *
 * Initialize the fields of the specified mutex.
 */
void
uthread_mtx_init(uthread_mtx_t *mtx)
{
  mtx->m_owner = NULL;
  utqueue_init(&mtx->m_waiters);
}



/*
 * uthread_mtx_lock
 *
 * Lock the mutex.  This call will block if it's already locked.  When the
 * thread wakes up from waiting, it should own the mutex (see _unlock()).
 * Mask preemption to ensure atomicity.
 */
void
uthread_mtx_lock(uthread_mtx_t *mtx)
{	
  uthread_nopreempt_on();	
  if(mtx->m_owner != NULL){
  	utqueue_enqueue(&mtx->m_waiters, ut_curthr);
  	uthread_block();
  	return;
  }
  mtx->m_owner = ut_curthr;
  uthread_nopreempt_off();
}


/*
 * uthread_mtx_trylock
 *
 * Try to lock the mutex, return 1 if we get the lock, 0 otherwise.
 * This call should not block. Mask preemption to ensure atomicity.
 */
int
uthread_mtx_trylock(uthread_mtx_t *mtx)
{
  uthread_nopreempt_on();
  if(mtx->m_owner != NULL){
  	uthread_nopreempt_off();
  	return 0;
  }
  mtx->m_owner = ut_curthr;
  uthread_nopreempt_off();
  return 1;
}


/*
 * uthread_mtx_unlock
 *
 * Unlock the mutex.  If there are people waiting to get this mutex,
 * explicitly hand off the ownership of the lock to a waiting thread and
 * then wake that thread. Mask preemption to ensure atomicity.
 */
void
uthread_mtx_unlock(uthread_mtx_t *mtx)
{
  uthread_nopreempt_on();
  if(!utqueue_empty(mtx->m_waiters)){
  	uthread_t *thread_to_giveM = utqueue_dequeue(&mtx->m_waiters);
  	mtx->m_owner = thread_to_giveM;
  } else {
  	mtx->m_owner = NULL;
  }
  
  uthread_nopreempt_off();
}
