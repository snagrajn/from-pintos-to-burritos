#include "vm/frame.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "threads/pte.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include <list.h>

/* Initializes the frame table and hand node. */
void
frame_init ()
{
  list_init (&frame_table);
  hand = NULL;
}

/* Obtains the frame details of the frame corresponding to the 
   page table entry PTE. */
void
get_frame (uint32_t *pte, struct frame_elem **f, struct pte_elem **p)
{
  struct list_elem *e; 

  for (e = list_begin (&frame_table); e != list_end (&frame_table);
       e = list_next (e))
    {
      struct frame_elem *fe = list_entry (e, struct frame_elem, elem);
      struct list *pte_list = &(fe->pte_list);
      struct list_elem *e_;
      for (e_ = list_begin (pte_list); e_ != list_end (pte_list);
           e_ = list_next (e_))
        {
          struct pte_elem *pe = list_entry (e_, struct pte_elem, elem);
          if (pte == pe->pte)
           {
             if (f != NULL) *f = fe; 
             if (p != NULL) *p = pe;
             return;
           }
        }
    }

  if (f != NULL) *f = NULL; 
  if (p != NULL) *p = NULL;
}

/* Evicts a VICTIM frame. */
void
evict (struct frame_elem *victim)
{
  if (victim->flags & FRAME_DIRTY)
   {
     /* The page is no longer an all zero page, 
        if it has become dirty. */ 
     if (victim->read_bytes == 0)
      {
        int i;
        uint8_t *page = ptov (victim->frame_addr);
        for (i = PGSIZE-1; i >= 0; i--)
            if (page[i] != 0)
               break;
        victim->read_bytes = i+1;
      }

     /* The page can no longer be read from an executable, 
        if it has become dirty. */ 
     victim->flags &= ~FRAME_EXEC;
   }

  swap_out (victim);

}

/* Update the status bits (accessed and dirty) of each frame. */
void 
frame_table_update ()
{
  struct list_elem *e;
  for (e = list_begin (&frame_table); e != list_end (&frame_table); 
       e = list_next (e))
    {
      struct frame_elem *f = list_entry (e, struct frame_elem, elem);
      f->flags &= ~(FRAME_DIRTY | FRAME_ACCESSED);

      struct list_elem *e_;
      for (e_ = list_begin (&f->pte_list); 
           (e_ != list_end (&f->pte_list)) && 
            !(f->flags & (FRAME_DIRTY | FRAME_ACCESSED));
           e_ = list_next (e_))
        {
          uint32_t *pte = list_entry (e_, struct pte_elem, elem)->pte;

          /* Set the dirty bit if the frame is found dirty. */
          if (*pte & PTE_D)
             f->flags |= FRAME_DIRTY;

          /* Set the accessed bit if the frame was recently referenced. */
          if (*pte & PTE_A)
             f->flags |= FRAME_ACCESSED;
        }
    }
}

/* Selects a frame for eviction from frame table using clock algorithm. */   
struct frame_elem * 
clock ()
{
  /* Update the status bits of all the frames in the frame table. */
  frame_table_update ();

  /* Initialize the hand node, if it is not. */
  if (hand == NULL) 
     hand = list_begin (&frame_table);

  struct list_elem *e;
  for (e = hand; ; e = list_next (e))
    {
      /* Wrap around if you reach the end of the list. */
      if (e == list_end (&frame_table)) 
         e = list_begin (&frame_table);

      struct frame_elem *f = list_entry (e, struct frame_elem, elem);

      /* A swapped out frame or a frame being swapped in, 
         does not participate in eviction. */
      if ((f->flags & FRAME_SWAP) || (f->flags & FRAME_IO))
         continue;

      /* Mark the frame as not referenced, if it was recently referenced. */
      if (f->flags & FRAME_ACCESSED)
         f->flags &= ~FRAME_ACCESSED;
      else
       {
         /* Call the next node as the hand node. */
         hand = list_next (e);

         /* Aliases of each frame must be updated to have the same
            value in their status bits. */
         sync_aliases ();

         /* Return the current node's frame as the victim frame. */
         return f;
       }
    }
}

/* Aliases of each frame must be updated to have the same value in their 
   status bits. */
void
sync_aliases ()
{
  struct list_elem *e;
  for (e = list_begin (&frame_table); e != list_end (&frame_table); 
       e = list_next (e))
     {
       struct frame_elem *f = list_entry (e, struct frame_elem, elem);

       struct list_elem *e_;
       for (e_ = list_begin (&f->pte_list); e_ != list_end (&f->pte_list);
            e_ = list_next (e_))
         {
           uint32_t *pte = list_entry (e_, struct pte_elem, elem)->pte;

           /* Set the dirty bit if the frame is found dirty. */
           (f->flags & FRAME_DIRTY) ?  (*pte |= PTE_D) : (*pte &= ~PTE_D);

           /* Set the accessed bit if the frame was recently referenced. */
           (f->flags & FRAME_ACCESSED) ?  (*pte |= PTE_A) : (*pte &= ~PTE_A);
         }
     }
}
