#include "vm/swap.h"
#include "threads/thread.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/pte.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "filesys/filesys.h"
#include <list.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Disk used for storing swapped out pages. */
struct disk *swap_disk;

/* Bitmap indicating free sectors on the swap disk. */
struct bitmap *swap_freemap = NULL;

/* Initializes the swap disk and creates a freemap of sectors 
   on the swap disk. */
void 
swap_init()
{
  /* Get the swap disk from hd1:1. */
  swap_disk = disk_get (0, 0);

  if (swap_disk == NULL)
     PANIC ("hd1:1 not present\n");

  /* Number of bits in the bitmap = Number of sectors. */
  swap_freemap = bitmap_create (40320);

  int i;
  for (i = 0; i < 30080; i++)
      bitmap_mark (swap_freemap, i);
}

void
swap_out (struct frame_elem *frame_elem)
{
  size_t sector_no = frame_elem->sector_no;
  void *page = ptov (frame_elem->frame_addr);

  frame_elem->flags |= FRAME_SWAP;

  enum intr_level level = intr_disable ();

  struct list_elem *e;
  for (e = list_begin (&frame_elem->pte_list);
       e != list_end (&frame_elem->pte_list);
       e = list_next (e))
    {
      uint32_t *pte = list_entry (e, struct pte_elem, elem)->pte;

      /* Clear the frame address entry in the PTE. */
      *pte &= PTE_FLAGS;

      /* Clear the present bit. */
      *pte &= ~PTE_P;
    }

  intr_set_level (level);

  struct disk *disk;
  
  if (!(frame_elem->flags & FRAME_DIRTY))
       goto done;
 
  else if (frame_elem->flags & FRAME_MMAP)
       disk = filesys_disk;               

  else
   {
     disk = swap_disk;

     /* Find 8 consecutive free sectors on the swap disk. */
     sector_no = bitmap_scan_and_flip (swap_freemap, 0, 8, false);
     if (sector_no == BITMAP_ERROR)
      {
        printf ("Out of virtual memory!!!!\n");
        return;
      }
     frame_elem->sector_no = sector_no;
   }

  /* Write the page sector by sector. */
  int i, read_bytes;
  read_bytes = (frame_elem->read_bytes > PGSIZE) ? 
                PGSIZE : frame_elem->read_bytes;

  for (i = 0; i < 8; i++)
    {
      disk_write (disk, sector_no + i, (uint8_t *)page + DISK_SECTOR_SIZE *i);
      read_bytes -= DISK_SECTOR_SIZE;
      if (read_bytes <= 0)
         break;
    }

done:

  /* Now, free the memory utilized by the page. */
  palloc_free_page (page);

}

void
swap_in (struct frame_elem *frame_elem)
{
  /* New page. */
  void *page = palloc_get_page (PAL_USER | PAL_ZERO);
  if (page == NULL)
   {
     evict (clock ());
     page = palloc_get_page (PAL_USER | PAL_ZERO);
   }

  size_t sector_no = frame_elem->sector_no;
  struct disk *disk;
  struct list_elem *e;

  frame_elem->frame_addr = vtop (page);
  
  if (frame_elem->read_bytes == 0)
     goto done;
  
  else if ((frame_elem->flags & FRAME_MMAP) ||
           (frame_elem->flags & FRAME_EXEC))
     disk = filesys_disk;         
      
  else
   {
     disk = swap_disk;

     /* Clear the bits used by the page being swapped in. */
     bitmap_set_multiple (swap_freemap, sector_no, 8, false);
     frame_elem->sector_no = 0;
   }

  /* Read the sectors onto the page, one by one. */
  int i, read_bytes;
  read_bytes = (frame_elem->read_bytes > PGSIZE) ? 
                PGSIZE : frame_elem->read_bytes;

  for (i = 0; i < 8; i++)
    {
      disk_read (disk, sector_no + i, (uint8_t *)page + DISK_SECTOR_SIZE *i);
      read_bytes -= DISK_SECTOR_SIZE;
      if (read_bytes <= 0)
         break;
    }

  if (frame_elem->read_bytes < PGSIZE)
     memset (page + frame_elem->read_bytes, 0, PGSIZE - frame_elem->read_bytes);

done:

  for (e = list_begin (&frame_elem->pte_list);
       e != list_end (&frame_elem->pte_list);
       e = list_next (e))
    {
      uint32_t *pte = list_entry (e, struct pte_elem, elem)->pte;

      /* Set the page table entry to FRAME_ADDR. */
      *pte |= frame_elem->frame_addr;

      /* Set the present bit. */
      *pte |= PTE_P;
    }

  frame_elem->flags &= ~FRAME_SWAP;
}
