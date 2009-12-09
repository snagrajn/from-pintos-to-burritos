#include "userprog/pagedir.h"
#include "threads/init.h"
#include "threads/thread.h"
#include "threads/interrupt.h"
#include "threads/pte.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "vm/swap.h"
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <bitmap.h>

static uint32_t *active_pd (void);
static void invalidate_pagedir (uint32_t *);

/* Creates a new page directory that has mappings for kernel
   virtual addresses, but none for user virtual addresses.
   Returns the new page directory, or a null pointer if memory
   allocation fails. */
uint32_t *
pagedir_create (void) 
{
  uint32_t *pd = palloc_get_page (0);
  if (pd != NULL)
    memcpy (pd, base_page_dir, PGSIZE);
  return pd;
}

extern struct bitmap *swap_freemap;
extern struct lock pg_fault_lock;
void pte_destroy (uint32_t *pte);

/* Destroys page directory PD, freeing all the pages it
   references. */
void
pagedir_destroy (uint32_t *pd) 
{
  uint32_t *pde;

  if (pd == NULL)
    return;

  ASSERT (pd != base_page_dir);

  for (pde = pd; pde < pd + pd_no (PHYS_BASE); pde++)
    if (*pde & PTE_P) 
      {
        uint32_t *pt = pde_get_pt (*pde);
        uint32_t *pte;
        
        for (pte = pt; pte < pt + PGSIZE / sizeof *pte; pte++)
          {
            lock_acquire (&pg_fault_lock);
            pte_destroy (pte); 
            lock_release (&pg_fault_lock);
          }
        palloc_free_page (pt);
      }
  palloc_free_page (pd);
}

/* Destroys page table entry PTE, freeing the page it
   references. */
void
pte_destroy (uint32_t *pte)
{
  struct frame_elem *f;
  struct pte_elem *pte_elem;

#ifdef VM
  frame_table_update ();

  get_frame (pte, &f, &pte_elem);

  if (f == NULL)
     return;

  /* Remove the page table entry from the frame element.*/ 
  list_remove (&pte_elem->elem);
  free (pte_elem);

  /* This frame is not shared by any other process. */
  if (list_empty (&f->pte_list))
   {
     /* If the frame is swapped out, free the sectors allocated to it*/
     if (f->flags & FRAME_SWAP)
         bitmap_set_multiple (swap_freemap, f->sector_no, 8, false);
     else
      {
        /* All dirty frames other than those of a memory mapped
           file should be discarded during eviction. */
        if (!(f->flags & FRAME_MMAP))
           f->flags &= ~FRAME_DIRTY;
        evict (f);
      }

     /* If this frame is the hand (used in clock algorithm for eviction), 
        then the next node will be made as the hand node. */
     if (&(f->elem) == hand)
        hand = list_next (hand);
     list_remove (&f->elem);
     free (f);
   }
#else
   palloc_free_page (pte_get_page (*pte));
#endif
}

/* Returns the address of the page table entry for virtual
   address VADDR in page directory PD.
   If PD does not have a page table for VADDR, behavior depends
   on CREATE.  If CREATE is true, then a new page table is
   created and a pointer into it is returned.  Otherwise, a null
   pointer is returned. */
uint32_t *
lookup_page (uint32_t *pd, const void *vaddr, bool create)
{
  uint32_t *pt, *pde;

  ASSERT (pd != NULL);

  /* Shouldn't create new kernel virtual mappings. */
  ASSERT (!create || is_user_vaddr (vaddr));

  /* Check for a page table for VADDR.
     If one is missing, create one if requested. */
  pde = pd + pd_no (vaddr);
  if (*pde == 0) 
    {
      if (create)
        {
          pt = palloc_get_page (PAL_ZERO);
          if (pt == NULL) 
            return NULL; 
      
          *pde = pde_create (pt);
        }
      else
        return NULL;
    }

  /* Return the page table entry. */
  pt = pde_get_pt (*pde);
  return &pt[pt_no (vaddr)];
}

/* Adds a mapping in page directory PD from user virtual page
   UPAGE to the physical frame identified by kernel virtual
   address KPAGE.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   FLAGS, READ_BYTES and SECTOR_NO are used to check if the 
   frame identified by the kernel vitual address page KPAGE is
   already present in the memory or one of the swap devices.
   If WRITABLE is true, the new page is read/write;
   otherwise it is read-only.
   Returns true if successful, false if memory allocation
   failed. */
bool
pagedir_set_page (uint32_t *pd, void *upage, void *kpage, 
                  bool writable, int flags, disk_sector_t sector_no, 
                  size_t read_bytes)
{
  uint32_t *pte = NULL;

  ASSERT (pg_ofs (upage) == 0);
  ASSERT (pg_ofs (kpage) == 0);
  ASSERT (is_user_vaddr (upage));
  ASSERT (vtop (kpage) >> PTSHIFT < ram_pages);
  ASSERT (pd != base_page_dir);

  pte = lookup_page (pd, upage, true);

  if (pte != NULL)
    {
      *pte = pte_create_user (kpage, writable);

#ifdef VM
  lock_acquire (&pg_fault_lock);

  if (flags & FRAME_SWAP)
     *pte &= ~PTE_P;  

  struct pte_elem *pte_elem = (struct pte_elem *)
                              malloc (sizeof (struct pte_elem));
  pte_elem->pte = pte;

  struct list_elem *e;

  for (e = list_begin (&frame_table); e != list_end (&frame_table); 
       e = list_next (e))
    {
      struct frame_elem *f = list_entry (e, struct frame_elem, elem);

      /* Check whether there is any such frame in the memory or in any of
         the swap devices (swap disk and filesystem disk). */
       if ((f->flags & (flags & ~FRAME_SWAP)) && (f->sector_no == sector_no)
           && (f->read_bytes >= read_bytes))
        {
          /* This page should not be shared. */
          if ((flags & FRAME_EXEC) && writable)
             break;

          /* If the frame is already present in the memory, then update
             the frame address in the page table entry of this page. */
          if (!(f->flags & FRAME_SWAP))
           {
             *pte |= f->frame_addr;
             *pte |= PTE_P;
           }

          frame_table_update ();

          /* Status bits of this page should be in sync with its aliases. */
          if (f->flags & FRAME_DIRTY)
             *pte |= PTE_D;
          if (f->flags & FRAME_ACCESSED)
             *pte |= PTE_A;

          list_push_back (&f->pte_list, &pte_elem->elem);
          lock_release (&pg_fault_lock);
          return true;
       }
    }

  /* If the required frame is not present in the memory or in any of the
     swap devices, create a new frame. */
  struct frame_elem *f = (struct frame_elem *) 
                          malloc (sizeof (struct frame_elem));
  f->frame_addr = *pte & ~PTE_FLAGS;
  f->sector_no = sector_no;
  f->flags = flags;
  f->read_bytes = read_bytes;
  list_init (&f->pte_list);
  list_push_back (&f->pte_list, &pte_elem->elem);
  list_push_back (&frame_table, &f->elem);
  lock_release (&pg_fault_lock);

#endif
      return true;
    }
  else
    return false;  
}

/* Looks up the physical address that corresponds to user virtual
   address UADDR in PD.  Returns the kernel virtual address
   corresponding to that physical address, or a null pointer if
   UADDR is unmapped. */
void *
pagedir_get_page (uint32_t *pd, const void *uaddr) 
{
  uint32_t *pte;

  ASSERT (is_user_vaddr (uaddr));
  
  pte = lookup_page (pd, uaddr, false);
  if (pte != NULL && (*pte & PTE_P) != 0)
     return pte_get_page (*pte) + pg_ofs (uaddr);
  else
    return NULL;
}

/* Marks user virtual page UPAGE "not present" in page
   directory PD.  Later accesses to the page will fault.  Other
   bits in the page table entry are preserved.
   UPAGE need not be mapped. */
void
pagedir_clear_page (uint32_t *pd, void *upage) 
{
  uint32_t *pte;

  ASSERT (pg_ofs (upage) == 0);
  ASSERT (is_user_vaddr (upage));

  pte = lookup_page (pd, upage, false);
  if (pte != NULL && (*pte & PTE_P) != 0)
    {
      *pte &= ~PTE_P;
      invalidate_pagedir (pd);
    }
}

/* Returns true if the PTE for virtual page VPAGE in PD is dirty,
   that is, if the page has been modified since the PTE was
   installed.
   Returns false if PD contains no PTE for VPAGE. */
bool
pagedir_is_dirty (uint32_t *pd, const void *vpage) 
{
  uint32_t *pte = lookup_page (pd, vpage, false);
  return pte != NULL && (*pte & PTE_D) != 0;
}

/* Set the dirty bit to DIRTY in the PTE for virtual page VPAGE
   in PD. */
void
pagedir_set_dirty (uint32_t *pd, const void *vpage, bool dirty) 
{
  uint32_t *pte = lookup_page (pd, vpage, false);
  if (pte != NULL) 
    {
      if (dirty)
        *pte |= PTE_D;
      else 
        {
          *pte &= ~(uint32_t) PTE_D;
          invalidate_pagedir (pd);
        }
    }
}

/* Returns true if the PTE for virtual page VPAGE in PD has been
   accessed recently, that is, between the time the PTE was
   installed and the last time it was cleared.  Returns false if
   PD contains no PTE for VPAGE. */
bool
pagedir_is_accessed (uint32_t *pd, const void *vpage) 
{
  uint32_t *pte = lookup_page (pd, vpage, false);
  return pte != NULL && (*pte & PTE_A) != 0;
}

/* Sets the accessed bit to ACCESSED in the PTE for virtual page
   VPAGE in PD. */
void
pagedir_set_accessed (uint32_t *pd, const void *vpage, bool accessed) 
{
  uint32_t *pte = lookup_page (pd, vpage, false);
  if (pte != NULL) 
    {
      if (accessed)
        *pte |= PTE_A;
      else 
        {
          *pte &= ~(uint32_t) PTE_A; 
          invalidate_pagedir (pd);
        }
    }
}

/* Loads page directory PD into the CPU's page directory base
   register. */
void
pagedir_activate (uint32_t *pd) 
{
  if (pd == NULL)
    pd = base_page_dir;

  /* Store the physical address of the page directory into CR3
     aka PDBR (page directory base register).  This activates our
     new page tables immediately.  See [IA32-v2a] "MOV--Move
     to/from Control Registers" and [IA32-v3a] 3.7.5 "Base
     Address of the Page Directory". */
  asm volatile ("movl %0, %%cr3" : : "r" (vtop (pd)) : "memory");
}

/* Returns the currently active page directory. */
static uint32_t *
active_pd (void) 
{
  /* Copy CR3, the page directory base register (PDBR), into
     `pd'.
     See [IA32-v2a] "MOV--Move to/from Control Registers" and
     [IA32-v3a] 3.7.5 "Base Address of the Page Directory". */
  uintptr_t pd;
  asm volatile ("movl %%cr3, %0" : "=r" (pd));
  return ptov (pd);
}

/* Some page table changes can cause the CPU's translation
   lookaside buffer (TLB) to become out-of-sync with the page
   table.  When this happens, we have to "invalidate" the TLB by
   re-activating it.

   This function invalidates the TLB if PD is the active page
   directory.  (If PD is not active then its entries are not in
   the TLB, so there is no need to invalidate anything.) */
static void
invalidate_pagedir (uint32_t *pd) 
{
  if (active_pd () == pd) 
    {
      /* Re-activating PD clears the TLB.  See [IA32-v3a] 3.12
         "Translation Lookaside Buffers (TLBs)". */
      pagedir_activate (pd);
    } 
}
