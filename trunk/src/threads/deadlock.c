#include "threads/deadlock.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/synch.h"

bool
someone_not_visited (int *visited, int size)
{
  int i;
  for (i = 0; i < size; i++)
      if (visited[i] == 0)
         return true;
  return false;
} 

bool
detect_cycles (int **graph, int order)
{
  int *visited = (int*) calloc (order, sizeof (int));
  int i, j, working_node;

  while (someone_not_visited (visited, order))
    {
      for (i = 0; i < order; i++)
        {
          if (visited[i]) continue;

          for (j = 0; j < order; j++)
              if (graph[j][i] != 0) break;

          if (j == order)
           {
             working_node = i;
             break;
           }
        }
      if (i == order)
       {
         free (visited);
         return true;
       }

      visited[working_node] = 1;
      for (i = 0; i < order; i++)
          graph[working_node][i] = 0;
    }

  free (visited);
  return false;
}       

bool
detect_deadlocks (struct thread *h, struct thread *w)
{
  int i,j;
  int size = list_size (&thread_list);

  if ((size <= 1) || (list_size (&lock_list) <= 1))
     return false;

  enum intr_level old_level = intr_disable ();

  int **wfg = (int**) calloc (size, sizeof (int*));
  for (i = 0; i < size; i++)
      *(wfg+i) = (int*) calloc (size, sizeof (int));
  
  struct list_elem *e, *f;

  for (e = list_begin (&lock_list); e != list_end (&lock_list); 
       e = list_next (e))
    {
      struct lock *l = list_entry (e, struct lock_elem, elem)->lock;
      if (l->holder == NULL)
         continue;
      i = get_id (l->holder);
      for (f = list_begin (&l->semaphore.waiters);
           f != list_end (&l->semaphore.waiters);
           f = list_next (f))
        {
          j = get_id (list_entry (f, struct thread, elem));
          wfg[j][i] = 1;
        }
    }

  i = get_id (h);
  j = get_id (w);
  wfg[j][i] = 1;

/*  for (i=0;i<size;i++){
  for (j=0;j<size;j++)
      printf ("%d ",wfg[i][j]);
  printf ("\n");
  }
*/
  if (detect_cycles (wfg, size))
   {
     free (wfg);
     intr_set_level (old_level);
     return true;
   }

  free (wfg);
  intr_set_level (old_level);
  return false;
}

int get_id (struct thread *t)
{
  struct list_elem *e;
  int count = 0;
  for (e = list_begin (&thread_list); e != list_end (&thread_list); 
       e = list_next (e))
    {
      struct thread *u = list_entry (e, struct thread_elem, elem)->t;
      if (t == u)
         return count;
      count++;
    }
  return -1;
}
