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

static thread_func func1, func2;

struct lock a, b, c;

extern lock_acquire_test (struct lock *);

void
test_deadlock_nest (void) 
{

  /* This test does not work with the MLFQS. */
  ASSERT (!thread_mlfqs);

  /* Make sure our priority is the default. */
  ASSERT (thread_get_priority () == PRI_DEFAULT);

  lock_init (&a);
  lock_init (&b);
  lock_init (&c);

  lock_acquire_test (&a);
  
  thread_create ("child 1", PRI_DEFAULT + 1, func1, NULL);

  lock_acquire_test (&b);
  
  msg ("main released lock %d.", b.id);
  lock_release (&b);

  msg ("main released lock %d.", a.id);
  lock_release (&a);

  msg ("main ends");
}

static void
func1 (void *aux) 
{
  lock_acquire_test (&b);
  
  thread_create ("child 2", PRI_DEFAULT + 2, func2, NULL);

  lock_acquire_test (&c);

  msg ("child 1 released lock %d.", b.id);
  lock_release (&b);

  msg ("child 1 ends");
}

static void
func2 (void *aux) 
{
  lock_acquire_test (&c);
  lock_acquire_test (&a);

  msg ("child 2 released lock %d.", a.id);
  lock_release (&a);

  msg ("child 2 released lock %d.", c.id);
  lock_release (&c);

  msg ("child 2 ends");
}

