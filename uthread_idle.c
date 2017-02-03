/*
 *   FILE: uthread_idle.c
 * AUTHOR: Peter Demoreuille
 *  DESCR: idling.  later for system call handling
 *   DATE: Thu Oct  4 15:24:18 2001
 *
 */

#include <sched.h>

#include "uthread.h"
#include "uthread_private.h"


/*
 * uthread_idle
 *
 * right now we just call linux's yield() function.
 * note that we cannot make system calls here, since it
 * is called from uthread_switch().
 */
void
uthread_idle(void)
{
    sched_yield();
}
