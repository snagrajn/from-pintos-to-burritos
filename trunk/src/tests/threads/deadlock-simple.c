/* The main thread acquires locks A and B, then it creates two
   higher-priority threads.  Each of these threads blocks
   acquiring one of the locks and thus donate their priority to
   the main thread.  The main thread releases the locks in turn
   and relinquishes its donated priorities.
   
   Based on a test originally submitted for Stanford's CS 140 in
   winter 1999 by Matt Franklin <startled@leland.stanford.edu>,
   Greg Hutchins <gmh@leland.stanford.edu>, Yu Ping Hu
   <yph@cs.stanford.edu>.  Modified by arens. */

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/init.h"
#include "threads/synch.h"
#include "threads/thread.h"

static thread_func func;

struct lock a, b;

void
lock_acquire_test (struct lock *lock)
{
  msg ("%s tries to acquire lock %d.", thread_current ()->name, lock->id);
  if (lock_acquire (lock))
     msg ("%s acquired lock %d.", thread_current ()->name, lock->id);
  else
     msg ("%s could not acquire lock %d.", thread_current ()->name, lock->id);
}

void
test_deadlock_simple (void) 
{

  /* This test does not work with the MLFQS. */
  ASSERT (!thread_mlfqs);

  /* Make sure our priority is the default. */
  ASSERT (thread_get_priority () == PRI_DEFAULT);

  lock_init (&a);
  lock_init (&b);

  lock_acquire_test (&a);
  
  thread_create ("child", PRI_DEFAULT + 1, func, NULL);

  lock_acquire_test (&b);
  
  msg ("main released lock %d.", a.id);
  lock_release (&a);

  msg ("main ends");
}

static void
func (void *aux) 
{
  lock_acquire_test (&b);
  lock_acquire_test (&a);

  msg ("child released lock %d.", a.id);
  lock_release (&a);

  msg ("child released lock %d.", b.id);
  lock_release (&b);

  msg ("child ends");
}

