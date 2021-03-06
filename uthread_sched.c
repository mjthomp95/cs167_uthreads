/*
 *   FILE: uthread_sched.c
 * AUTHOR: Peter Demoreuille
 *  DESCR: scheduling wack for uthreads
 *   DATE: Mon Oct  1 00:19:51 2001
 *
 * Modified to handle time slicing by Tom Doeppner
 *   DATE: Sun Jan 10, 2016
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <signal.h>
#include <sys/time.h>

#include "uthread.h"
#include "uthread_private.h"
#include "uthread_ctx.h"
#include "uthread_queue.h"
#include "uthread_bool.h"
#include "uthread_sched.h"


/* ---------- globals -- */

int clock_count;
int taken_clock_count;

static utqueue_t runq_table[UTH_MAXPRIO + 1]; /* priority runqueues */
static int uthread_no_preempt;              /* preemption not allowed */
static int uthread_no_preempt_count;		/* used to allow nested calls to uthread_no_preempt_on */


/* ----------- public code -- */
//Prototype
int runq_nonempty(void);

/*
 * uthread_yield
 *
 * Causes the currently running thread to yield use of the processor to
 * another thread. The thread is still runnable however, so it should
 * be in the UT_RUNNABLE state and schedulable by the scheduler. When this
 * function returns, the thread should be executing again. A bit more clearly,
 * when this function is called, the current thread stops executing for some
 * period of time (allowing another thread to execute). Then, when the time
 * is right (ie when a call to uthread_switch() results in this thread
 * being swapped in), the function returns.
 */
void
uthread_yield(void)
{
  uthread_nopreempt_on();
  if(runq_nonempty()){
    utqueue_enqueue(runq_table[&ut_curthr->ut_prio], ut_curthr);
    ut_curthr->ut_state = UT_RUNNABLE;
    uthread_switch();
  } else {
    uthread_nopreempt_off();
  }
  return;
}



/*
 * uthread_block
 *
 * Put the current thread to sleep, pending an appropriate call to
 * uthread_wake().
 */
void
uthread_block(void)
{
  uthread_nopreempt_on();
  ut_curthr->ut_state = UT_WAIT;
  uthread_switch();
}


/*
 * uthread_wake
 *
 * Wakes up the supplied thread (schedules it to be run again).  The
 * thread may already be runnable or (well, if uthreads allowed for
 * multiple cpus) already on cpu, so make sure to only mess with it if
 * it is actually in a wait state. Note that if the target thread has
 * a higher priority than the caller does, the caller should yield.
 */
void
uthread_wake(uthread_t *uthr)
{
  uthread_nopreempt_on();
  if(uthr->ut_state == UT_WAIT){
    utqueue_enqueue(runq_table[uthr->ut_prio], uthr);
    uthr->ut_state == UT_RUNNABLE;
  }
  
  if(uthr->ut_prio > ut_curthr->ut_prio){
    uthread_yield();
  } else {
    uthread_nopreempt_off();
  }
}


/*
 * uthread_setprio
 *
 * Changes the priority of the indicated thread.  Note that if the thread
 * is in the UT_RUNNABLE state (it's runnable but not on cpu) you should
 * change the list it's waiting on so the effect of this call is
 * immediate. Yield to the target thread if its priority is higher than
 * the caller's.
 */
int
uthread_setprio(uthread_id_t id, int prio)
{
  uthread_nopreempt_on();
  uthread_t thread_to_change = uthreads[id];
  if(thread_to_change.ut_state == UT_NO_STATE || thread_to_change.ut_state == UT_TRANSITION){
    return -1;
  }
  int pre_prio = thread_to_change.ut_prio;
  thread_to_change.ut_prio = prio;
  if(thread_to_change.ut_state == UT_RUNNABLE){
    utqueue_remove(runq_table[pre_prio], &thread_to_change);
    utqueue_enqueue(runq_table[prio], &thread_to_change);
  }
  uthread_nopreempt_off();
  if(thread_to_change.ut_prio > ut_curthr->ut_prio){
    uthread_yield();
  } 
  return 0;
}

/*
 * uthread_no_preempt_on
 *
 * Disable preemption. Uses a global mask rather than making sys calls.
 */
void uthread_nopreempt_on(void) {
  uthread_no_preempt_count++;
  uthread_no_preempt = 1;
}

void uthread_nopreempt_off(void) {
  if (--uthread_no_preempt_count == 0)
    uthread_no_preempt = 0;
  assert(uthread_no_preempt_count >= 0);
}



/* ----------- private code -- */

//Checks to see if all the priority queues are empty. If not, it returns the
//highest non-empty runq according to priority otherwise it returns 0.
int
runq_nonempty()
{
  for(int i = UTH_MAXPRIO; i >= 0; i--){
    if(!utqueue_empty(&runq_table[i])){
      return i;
    }
  }
  return 0;
}

/*
 * uthread_switch()
 *
 * This is where all the magic is.  Wait until there is a runnable thread, and
 * then switch to it using uthread_swapcontext().  Make sure you pick the
 * highest priority runnable thread to switch to. Also don't forget to take
 * care of setting the ON_CPU thread state and the current thread. Note that
 * it is okay to switch back to the calling thread if it is the highest
 * priority runnable thread.
 *
 * Every time uthread_switch() is called, uthread_idle() should be called at
 * least once.  In addition, when there are no runnable threads, you should
 * repeatedly call uthread_idle() until there are runnable threads.  Threads
 * with numerically higher priorities run first. For example, a thread with
 * priority 8 will run before one with priority 3.
 * */
void
uthread_switch()
{
  uthread_nopreempt_on();
  int runq_index;
  do{
    uthread_idle();
  }while(!(runq_index = runq_nonempty()))
  thread_t *thread_to_run = utqueue_dequeue(runq_table[runq_index]);
  thread_to_run->ut_state = UT_ON_CPU;
  thread_t *switched_thread = ut_curthr;
  ut_curthr = thread_to_run;
  uthread_nopreempt_reset();
  uthread_swapcontext(&switched_thread->ut_ctx, &ut_curthr->ut_ctx);
}


void uthread_start_timer(void);
/*
 * uthread_sched_init
 *
 * Setup the scheduler. This is called once from uthread_init().
 * This also kicks off the time-slice timer.
 */
void
uthread_sched_init(void)
{
  for(int i = 0; i < UTH_MAXPRIO + 1; i++){
    utqueue_init(&runq_table[i]);
  }
}

static void clock_interrupt(int);
static sigset_t VTALRMmask;

/*
 * uthread_start_timer
 *
 * Start up the time-slice clock, which uses ITIMER_VIRTUAL and SIGVTALRM.
 * For test purposes, the time-slice interval is as small as possible
 */
void
uthread_start_timer(void) {
  struct sigaction timesliceact; 
  timesliceact.sa_handler = clock_interrupt; 
  timesliceact.sa_mask = VTALRMmask; 
  timesliceact.sa_flags = SA_RESTART; // avoid EINTR 
  struct timeval interval = {0, 1}; // every .001 milliseconds 
  struct itimerval timerval; 
  timerval.it_value = interval; 
  timerval.it_interval = interval; 
  sigaction(SIGVTALRM, &timesliceact, 0); 
  setitimer(ITIMER_VIRTUAL, &timerval, 0); 
  sigemptyset(&VTALRMmask); 
  sigaddset(&VTALRMmask, SIGVTALRM); 
}

/*
 * clock_interrupt
 *
 * At each clock interrupt (SIGVTALRM), call uthread_yield if preemption is not masked.
 * When implementing this function it is important to understand that uthreads
 * execute within pthreads. Make sure you are clear on what constructs are
 * pthread-specific, and what constructs are uthread-specific.
 */
static void
clock_interrupt(int sig) {
  clock_count++;
  if(uthread_no_preempt)
    return;
  taken_clock_count++;
  sigprocmask(SIG_UNBLOCK, &VTALRMmask, 0);
  uthread_yield();
}

/*
 * uthread_nopreempt_reset
 *
 * Allow preemption, regardless of the nesting level.
 */
void uthread_nopreempt_reset(void) {
  uthread_no_preempt_on = 0;
  uthread_no_preempt_count = 0;
}
