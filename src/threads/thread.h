#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/synch.h"
#include "devices/timer.h"

/* States in a thread's life cycle. */
enum thread_status
  {
    THREAD_RUNNING,     /* Running thread. */
    THREAD_READY,       /* Not running but ready to run. */
    THREAD_BLOCKED,     /* Waiting for an event to trigger. */
    THREAD_DYING        /* About to be destroyed. */
  };

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */
#define LOAD_ERROR -2                   /* Error value in case of 
                                           unsuccessful load. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

/* User Stack Limit. */
#define STACK_LIMIT (8*1024*1024)

/* A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion. */

/* The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list. */
struct thread
  {
    /* Owned by thread.c. */
    tid_t tid;                          /* Thread identifier. */
    enum thread_status status;          /* Thread state. */
    char name[16];                      /* Process name omitting 
                                           command line. */
    uint8_t *stack;                     /* Saved stack pointer. */
    int old_priority;                   /* Priority before donation. */
    int priority;                       /* Priority. */
    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element. */
#ifdef USERPROG
    /* Owned by userprog/process.c. */
    uint32_t *pagedir;                  /* Page directory. */
    int user_stack_size;                /* Size of the user stack. 
                                           Initial size = 4KB. Can grow
                                           upto STACK_LIMIT (8MB). */
    struct list children;               /* List of children. */
    struct semaphore wait;		/* Semaphore for signalling 
					   waiting parent. */
    struct semaphore zombie;		/* Semaphore for waiting for 
					   a signal from parent in 
                                           zombie state (before death). */
    struct file *executable;            /* Executable having this 
					   thread's instructions. */
    int load_status;                    /* A status of 0 indicates that the
                                           thread was successful in loading
                                           its executable and nonzero 
                                           indicates errors. */
    int exit_status;                    /* Exit status of the thread. 
                                           A status of 0 indicates success
                                           and nonzero indicates errors. */
    struct list fd_list;                /* List of file descriptor elements. */
#endif
    int64_t nice;                       /* Niceness. */
    int64_t recent_cpu;                 /* Recent CPU Time. */
    /* Owned by thread.c. */
    unsigned magic;                     /* Detects stack overflow. */
    struct dir *current_directory;      /* Current working directory for this
                                           thread. */
  };

/* One thread in a list of all alive threads. */
struct thread_elem
  {
    struct thread *t;			/* This thread. */
    struct list_elem elem;              /* List element. */
  };

/* List of all threads that are alive. */
struct list thread_list;

#ifdef USERPROG
/* One child in a list of children. */
struct child
  {
    struct thread *thread;		/* This thread. */
    struct list_elem elem;		/* List element. */
  };

struct file_desc
 {
   int fd;				/* File Descriptor. */
   struct file *file;			/* File Pointer */
   struct list_elem elem;		/* List Element. */
 };
#endif

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

/* Estimate of the average number of
   threads ready to run over the past minute. */
static int64_t load_average = 0;  

/* If false (default), memory not intialized.
   If true, memory intialized.
   Memory gets intialized during boot action. */
bool mem_initialized;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

list_less_func priority_less;
list_less_func lock_less;
list_less_func cond_less;
#endif /* threads/thread.h */
