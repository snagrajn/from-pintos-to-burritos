#include <list.h>
#include "devices/disk.h"

/* Type of the frame.  */
enum frame_flags
  {
    FRAME_MMAP       = 001,         /* Frame in a memory mapped file. */
    FRAME_EXEC       = 002,         /* Frame in an executable. */
    FRAME_SWAP       = 004,         /* A swapped out frame. */
    FRAME_DIRTY      = 010,	    /* A dirty frame (modified). */
    FRAME_ACCESSED   = 020,         /* A recently referenced frame. */
    FRAME_IO         = 040          /* Frame being read/written from/into 
                                       the disk. */
  };

/* An entry in the frame table. */
struct frame_elem
  {
    uintptr_t frame_addr;          /* Address of this frame. */ 
    struct list pte_list;          /* List of all PTEs sharing this frame. */
    struct list_elem elem;         /* This element. */
    int flags;		           /* Flags indicating type and
                                      present status of the frame. */
    disk_sector_t sector_no;       /* Sector number of the swap device. */
    size_t read_bytes;             /* File length if the page contains a
                                      memory mapped file. 
                                      Number of non-zero bytes in the frame,
                                      otherwise. */
  };

/* List of all frames. */
struct list frame_table;

/* An entry in the pte_list of a frame table element. */
struct pte_elem
  {
    uint32_t *pte;                 /* Pointer to Page Table Entry. */
    struct list_elem elem;         /* This element. */
  };

/* Hand element in the clock algorithm. */
struct list_elem *hand;

void frame_init (void);
void frame_table_update (void);
void sync_aliases (void);
struct frame_elem *clock (void);
void evict (struct frame_elem *);
void get_frame (uint32_t *, struct frame_elem **, struct pte_elem **); 
