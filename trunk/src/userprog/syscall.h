#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/init.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "devices/input.h"
#include "userprog/process.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "filesys/directory.h"
#include "filesys/inode.h"
#include <list.h>
#include <string.h>

void syscall_init (void);

#endif /* userprog/syscall.h */
