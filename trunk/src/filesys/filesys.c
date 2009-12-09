#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "threads/thread.h"
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "devices/disk.h"

/* The disk that contains the file system. */
struct disk *filesys_disk;
char *last = NULL, *last_but_one = NULL;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  filesys_disk = disk_get (0, 0);
  
  if (filesys_disk == NULL)
    PANIC ("hd0:1 (hdb) not present, file system initialization failed");

  inode_init ();
  free_map_init ();

  if (format) 
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
}

struct dir *
extract_directory (char **filename)
{
   struct dir *dir = NULL;
   char *str = malloc (strlen (*filename) + 1);
   strlcpy (str, *filename, strlen (*filename) + 1);

   if (*filename[0] == '/')
    {
      /* Absolute path name. */
      dir = dir_open_root();
      if (str[1] == '\0')
         str[0] = '.';
      else
         str += 1;
    }

   else
    {
      /* Relative path name. */
      dir = dir_reopen (thread_current ()->current_directory);
    }
         
   char delim[2] = "/";
   char *token1 = NULL, *token2 = NULL;
   char *saveptr;
    
   token1 = strtok_r (str, delim, &saveptr);
   if (token1 == NULL)
    {
      free (str);
      return NULL;
    }

   while (1)
    {
      str = NULL;
      token2 = strtok_r (str, delim, &saveptr);

      if (token2 != NULL)
       {
         struct dir_entry d_e;
         struct inode *inode = NULL;
         dir_lookup (dir, token1, &inode, &d_e);
         if (inode != NULL)
            dir = dir_open (inode_open (d_e.inode_sector));
         else
          {
            free (str);
            return NULL;       /* Failed lookup. */ 
          }
       }
      else
       {
         *filename = token1;
         free (str);
         return dir;
       }

       token1 = token2;
    }
 }

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size) 
{
  disk_sector_t inode_sector = 0;
  struct dir *dir = extract_directory ((char**)&name);  
      
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size)
                  && dir_add (dir, name, inode_sector));
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  
  dir_close (dir);
  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  struct dir *dir = extract_directory ((char**)&name);
  struct inode *inode = NULL;
  struct dir_entry e;

  if (dir != NULL)
   {
     dir_lookup (dir, name, &inode, &e);
     if (inode == NULL)
      {
        char *str = malloc (strlen (name) + 6);
        strlcpy (str, "/bin/", 6);
        strlcat (str, name, strlen (name) + 6);
        name = str;
        dir = extract_directory (&str);
        if (dir != NULL)
           dir_lookup (dir, str, &inode, &e);
        free (name);
      }
   }

  dir_close (dir);

  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  struct dir *dir = extract_directory ((char**)&name);
  bool success = dir != NULL && dir_remove (dir, name);
  dir_close (dir); 

  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  struct dir *dir = dir_open_root ();
  dir_add (dir, ".", ROOT_DIR_SECTOR);
  dir_add (dir, "..", ROOT_DIR_SECTOR);

  inode_set_isdir (dir->inode);  
  dir_close (dir);
  free_map_close ();
  printf ("done.\n");
}

void
chdir (const char *name)
{
  struct file *file = filesys_open (name);
  if (file == NULL)
   {
     printf ("cd : %s : no such file or directory.\n", name);
     return;
   }
  if (!inode_isdir (file->inode))
   {
     printf ("cd : %s : not a directory.\n", name);
     return;
   }
  struct dir *dir = dir_open (file->inode);
  inode_reopen (file->inode);
  file_close (file);
  dir_close (thread_current ()->current_directory);
  thread_current ()->current_directory = dir;
}
