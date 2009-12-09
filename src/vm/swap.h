#include "frame.h"
#include "devices/disk.h"
#include <bitmap.h>
#define PTE_ELEM_SIZE sizeof (struct pte_elem)

void swap_init(void);

void swap_out (struct frame_elem *);

void swap_in (struct frame_elem *);
