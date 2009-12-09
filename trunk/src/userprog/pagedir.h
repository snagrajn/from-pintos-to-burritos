#ifndef USERPROG_PAGEDIR_H
#define USERPROG_PAGEDIR_H

#include "devices/disk.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

uint32_t *pagedir_create (void);
void pagedir_destroy (uint32_t *pd);
uint32_t *lookup_page (uint32_t *, const void *, bool);
bool pagedir_set_page (uint32_t *pd, void *upage, void *kpage, 
                       bool rw, int flags, disk_sector_t sector_no, 
                       size_t read_bytes);
void *pagedir_get_page (uint32_t *pd, const void *upage);
void pagedir_clear_page (uint32_t *pd, void *upage);
bool pagedir_is_dirty (uint32_t *pd, const void *upage);
void pagedir_set_dirty (uint32_t *pd, const void *upage, bool dirty);
bool pagedir_is_accessed (uint32_t *pd, const void *upage);
void pagedir_set_accessed (uint32_t *pd, const void *upage, bool accessed);
void pagedir_activate (uint32_t *pd);

#endif /* userprog/pagedir.h */
