#ifndef FILESYS_FILESYS_H
#define FILESYS_FILESYS_H

#include <stdbool.h>
#include "filesys/off_t.h"

/* Sectors of system file inodes. */
#define FREE_MAP_SECTOR 200       /* Free map file inode sector. */
#define ROOT_DIR_SECTOR 201       /* Root directory file inode sector. */

/* Disk used for file system. */
extern struct disk *filesys_disk;
struct dir;

void filesys_init (bool format);
void filesys_done (void);
struct dir *extract_directory (char **filename);
bool filesys_create (const char *name, off_t initial_size);
struct file *filesys_open (const char *name);
bool filesys_remove (const char *name);
void chdir (const char *name);

#endif /* filesys/filesys.h */
