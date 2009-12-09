#include "vm/mmap.h"
#include "vm/frame.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/pte.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "userprog/pagedir.h"
#include "filesys/filesys.h"
#include "filesys/file.h"

mapid_t
mmap (int fd, void *addr)
{
  struct thread *cur = thread_current ();
  struct list *fd_list = &(cur->fd_list);
  struct list_elem *e;
  struct file_desc *file_d = NULL;

  /* Console I/O cannot be mapped. */
  if (fd == 0 || fd == 1)
     return -1;

  /* Virtual page 0 is not mapped in Pintos. */
  if (addr == 0)
     return -1;

  /* ADDR is not page-aligned. */
  if ((uint32_t)addr % PGSIZE)
     return -1;

  for (e = list_begin (fd_list); e != list_end (fd_list);
       e = list_next (e))
    {
      file_d = list_entry (e, struct file_desc, elem);
      if (file_d->fd == fd)
       {
	 int flength = file_length (file_d->file);

         /* FD has a length of zero bytes. */
         if (flength == 0)
            return -1;

         uint32_t *pd = cur->pagedir;
         uint32_t *pte;
         uint8_t *page;
         bool writable = !(file_d->file->deny_write); 
         void *kpage = ptov (0);
         disk_sector_t sector_no = byte_to_sector (file_d->file->inode, 0);

         for (page = addr; page < (uint8_t *)addr + flength; 
              page += PGSIZE, sector_no += (PGSIZE/DISK_SECTOR_SIZE))
           {
             /* Check whether the page is already mapped. */
             if (((pte = lookup_page (pd, page, false)) != NULL) &&
                 (*pte & PTE_U))
                return -1;

             if (!pagedir_set_page (pd, page, kpage, writable, 
                                    FRAME_MMAP | FRAME_SWAP, sector_no, 
                                    flength))
                return -1;
           }

         /* Re-open the inode, so that the file still exists on the disk if 
            the user tries to remove it.*/
         inode_reopen (file_d->file->inode);
         
         /* Virtual address is returned as Mapping ID. */
         return addr;
       }
    }
  /* We arrive here only if FD is not valid in the current process's context. */
  return -1;
}

void
munmap (mapid_t mapping)
{
  struct thread *cur = thread_current ();
  uint32_t *pd = cur->pagedir;
  uint8_t *page;
  int flength = PGSIZE;

  for (page = mapping; page < (uint8_t *)mapping + flength; page += PGSIZE)
    { 
      uint32_t *pte = lookup_page (pd, page, false);

      struct list_elem *e;
      struct frame_elem *f;
      struct pte_elem *pte_elem;

      get_frame (pte, &f, &pte_elem);

      struct inode *inode = inode_open (f->sector_no);

      /* Close the inode twice. Once for the above open and once for the open
         in mmap. */
      inode_close (inode); 
      inode_close (inode); 
      
      if (page == mapping)
         flength = f->read_bytes;

      if (!(f->flags & FRAME_SWAP))
       {
         frame_table_update ();
         evict (f);
       }
 
      list_remove (&pte_elem->elem);
      free (pte_elem);
      return;
    }
}
