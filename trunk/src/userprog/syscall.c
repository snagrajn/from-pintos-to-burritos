#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

#ifdef VM
#include "vm/mmap.h"
#include "vm/swap.h"
#endif

static void syscall_handler (struct intr_frame *);
static int get_user (const uint8_t *);
static bool put_user (uint8_t *, uint8_t);
static bool strcopy (uint8_t *, uint8_t *, uint32_t size);
static bool valid (char *);
static void exit_on_badarg (uint32_t*, int);

extern struct lock pg_fault_lock;

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f ) 
{
  uint32_t *sp = f->esp;
  if (get_user (sp) == -1)
   {
     thread_current ()->exit_status = -1;
     thread_exit ();
   }
  struct list *fd_list = &thread_current ()->fd_list;
  uint32_t sysno = *sp;

  switch (sysno)
    {
      case SYS_HALT :                  
        power_off();
      
      case SYS_EXIT :
        {
          exit_on_badarg (sp, 1);
          int status = *(sp + 1);
          thread_current ()->exit_status = status;
          thread_exit ();
        }

      case SYS_EXEC :
        {
          exit_on_badarg (sp, 1);
          char *cmd_line = (char*)*(sp + 1);
          if (!valid (cmd_line))
           {
             thread_current ()->exit_status = -1;
             thread_exit ();
           }
          f->eax = process_execute (cmd_line);
          break;
        }

      case SYS_WAIT :
        {          
          exit_on_badarg (sp, 1);

          /* Process ID and Thread ID are used interchangeably since
             all processes in Pintos are single-threaded. */
          tid_t tid = (tid_t)*(sp + 1);
          f->eax = process_wait (tid);
          break;
        }

      case SYS_CREATE : 
        { 
          exit_on_badarg (sp, 2);
          char *filename = (char *)*(sp + 1);
          if (!valid (filename))
           {
             thread_current ()->exit_status = -1;
             thread_exit ();
           }
          unsigned initial_size = (unsigned) *(sp + 2);
          f->eax = filesys_create (filename, initial_size);
          break; 
        }

      case SYS_REMOVE :
        { 
          exit_on_badarg (sp, 1);
          char *filename = (char *)*(sp + 1);
          if (!valid (filename))
           {
             thread_current ()->exit_status = -1;
             thread_exit ();
           }
          f->eax = filesys_remove (filename);
          break;
        }

      case SYS_OPEN :   
        {
          exit_on_badarg (sp, 1);
          char *filename = (char *)*(sp + 1);
          if (!valid (filename))
           {
             thread_current ()->exit_status = -1;
             thread_exit ();
           }
          struct file *file = filesys_open (filename);
          if (file == NULL)
           {
             f->eax = -1;
             return;
           }
          struct file_desc *file_d = (struct file_desc *)
                                     malloc (sizeof (struct file_desc));

          int fd;
          if (list_empty (fd_list))
             fd = 2;
          else
             fd = list_entry (list_begin (fd_list), struct file_desc, elem)->fd;

          if (fd > 3)
           {
             file_d->fd = fd - 1;
             list_push_front (fd_list, &file_d->elem);
           }
          else
           {
             if (!list_empty (fd_list))
                fd = list_entry (list_rbegin (fd_list), 
                                 struct file_desc, elem)->fd;
             file_d->fd = fd + 1;
             list_push_back (fd_list, &file_d->elem);
           } 
          file_d->file = file;
          f->eax = file_d->fd;
          break; 
        }

      case SYS_CLOSE :
        { 
          exit_on_badarg (sp, 1);
          int fd = *(sp + 1);
          struct list_elem *e;
          struct file_desc *file_d = NULL;
          for (e = list_begin (fd_list); e != list_end (fd_list); 
               e = list_next (e))
            {
              file_d = list_entry (e, struct file_desc, elem);
              if (file_d->fd == fd)
               {
                 file_close (file_d->file);
                 list_remove (e);
                 free (file_d);
                 break;
               }
            }
          break; 
        }

      case SYS_FILESIZE :
        { 
          exit_on_badarg (sp, 1);
          int fd = *(sp + 1);
          struct list_elem *e;
          struct file_desc *file_d = NULL;
          f->eax = -1;
          for (e = list_begin (fd_list); e != list_end (fd_list); 
               e = list_next (e))
            {   
              file_d = list_entry (e, struct file_desc, elem);
              if ((file_d->fd == fd) && (!inode_isdir (file_d->file->inode)))
               {
                 f->eax = file_length (file_d->file);
                 break;
               }
            }
          break;
        }

      case SYS_READ :
        { 
          exit_on_badarg (sp, 3);
          int fd = *(sp + 1);
          char *u_buffer = (char *)*(sp + 2);
          unsigned size = *(sp + 3);
          if (size <= 0)
           {
             f->eax = 0;
             return;
           }
          char *k_buffer = (char *) malloc (size + 1);
          unsigned count = 0;
          if (fd == 0)
           {
             while (count < size)
               {
                 uint8_t ch = input_getc ();
                 if (ch == '\n')
                 break;
                 put_user (u_buffer + count, ch);
                 count++;
               }
             f->eax = count;
             return;
           }
          f->eax = -1;
          struct list_elem *e;
          struct file_desc *file_d = NULL;
          for (e = list_begin (fd_list); e != list_end (fd_list); 
               e = list_next (e))
            {
              file_d = list_entry (e, struct file_desc, elem);
              if ((file_d->fd == fd) && (!inode_isdir (file_d->file->inode)))
               { 
                 count = file_read (file_d->file, k_buffer, size);
                 if (strcopy (u_buffer, k_buffer, size))
                    f->eax = count;
                 else
                  {
                    thread_current ()->exit_status = -1;
                    free (k_buffer);
                    thread_exit ();
                  }
                 break;
               }
            }
          free (k_buffer);
          break; 
        }
 
      case SYS_WRITE :
        {
          exit_on_badarg (sp, 3);
          int fd = *(sp + 1);
          char *buffer = (char *)*(sp + 2);
          unsigned size = *(sp + 3);
          if (!valid (buffer))
           {
             thread_current ()->exit_status = -1;
             thread_exit ();
           }
          if (size <= 0)
           {
             f->eax = 0;
             return;
           }
          if (fd == 1)
           {
             putbuf (buffer, size);
             f->eax = size;
             return;
           }
          f->eax = -1;
          struct list_elem *e;
          struct file_desc *file_d = NULL;
          for (e = list_begin (fd_list); e != list_end (fd_list); 
               e = list_next (e))
            {
              file_d = list_entry (e, struct file_desc, elem);
              if ((file_d->fd == fd) && (!inode_isdir (file_d->file->inode)))
               {
                 unsigned count = 0;
                 count = file_write (file_d->file, buffer, size);
                 f->eax = count;
                 break;
               }
            }
          break; 
        }

      case SYS_SEEK :
        { 
          exit_on_badarg (sp, 2);
          int fd = *(sp + 1);
          unsigned position = *(sp + 2);
          struct list_elem *e;
          struct file_desc *file_d = NULL;
          for (e = list_begin (fd_list); e != list_end (fd_list); 
               e = list_next (e))
            {
              file_d = list_entry (e, struct file_desc, elem);
              if ((file_d->fd == fd) && (!inode_isdir (file_d->file->inode)))
               {
                 file_seek (file_d->file, position);
                 break;
               }
            }
          break;
        }   

      case SYS_TELL :
        { 
          exit_on_badarg (sp, 1);
          int fd = *(sp + 1);
          struct list_elem *e;
          struct file_desc *file_d = NULL;
          f->eax = -1;
          for (e = list_begin (fd_list); e != list_end (fd_list); 
               e = list_next (e))
            {
              file_d = list_entry (e, struct file_desc, elem);
              if ((file_d->fd == fd) && (!inode_isdir (file_d->file->inode)))
               {
                 f->eax = file_tell (file_d->file);
                 break; 
               }
            }
          break;
        }
 
#ifdef VM

      case SYS_MMAP :
        { 
          int fd = *(sp + 1);
          void *addr = *(sp + 2);
          f->eax = mmap (fd, addr);
          break;
        }

      case SYS_MUNMAP :
        { 
          mapid_t mapping = *(sp + 1);
          munmap (mapping);
          break;
        }

#endif

      case SYS_CHDIR :
        {
          exit_on_badarg (sp, 1);
          char *name = (char *)*(sp + 1);
          if (!valid (name))
           {
             thread_current ()->exit_status = -1;
             thread_exit ();
           } 
          
          f->eax = false;
          chdir (name);
          f->eax = true;
          break;
        }

      case SYS_MKDIR :
        {
          exit_on_badarg (sp, 1);
          char *name = (char *)*(sp + 1);
          if (!valid (name))
           {
             thread_current ()->exit_status = -1;
             thread_exit ();
           } 

	  bool success = filesys_create (name, DISK_SECTOR_SIZE);
          if (success)
           {
             struct file *file = filesys_open (name);
             inode_reopen (file->inode);
             struct dir *dir = dir_open (file->inode);
             dir_add (dir, ".", inode_get_inumber (file->inode));

             struct dir *parent = extract_directory (&name);
             dir_add (dir, "..", inode_get_inumber (parent->inode));

             inode_set_isdir (file->inode);
             file_close (file);
             dir_close (dir);
           }
          f->eax = success;
          break;
        }

      case SYS_READDIR :
        {
          exit_on_badarg (sp, 2);
          int fd = *(sp + 1);
          char *name = (char *)*(sp + 2);

          f->eax = false;
          struct list_elem *e;
          for (e = list_begin (fd_list); e != list_end (fd_list);
               e = list_next (e))
            {
              struct file_desc *file_d = list_entry (e, struct file_desc, elem);
              if (file_d->fd == fd)
               {
                 struct dir *dir = dir_open (file_d->file->inode);
                 inode_reopen (file_d->file->inode);
                 dir->pos = file_d->file->pos;
                 char *kname = (char *) malloc (NAME_MAX + 1);
                 f->eax = dir_readdir (dir, kname);
                 if (f->eax)
                    file_d->file->pos = dir->pos;
                 dir_close (dir);
                 if (!strcopy (name, kname, NAME_MAX +1))
                  {
                    thread_current ()->exit_status = -1;
                    free (kname);
                    thread_exit ();
                  }
                 free (kname);
                 break;
               }
            }
          break;
        }
          
      case SYS_ISDIR :
        {
          exit_on_badarg (sp, 1);
          int fd = *(sp + 1);
          f->eax = false;
          struct list_elem *e;
          for (e = list_begin (fd_list); e != list_end (fd_list); 
               e = list_next (e))
            {
              struct file_desc *file_d = list_entry (e, struct file_desc, elem);
              if (file_d->fd == fd)
               {
                 f->eax = inode_isdir (file_d->file->inode);
                 break; 
               }
            } 
          break;
        }
 
      case SYS_INUMBER :
        {
          exit_on_badarg (sp, 1);
          int fd = *(sp + 1);
          f->eax = -1;
          struct list_elem *e;
          for (e = list_begin (fd_list); e != list_end (fd_list); 
               e = list_next (e))
            {
              struct file_desc *file_d = list_entry (e, struct file_desc, elem);
              if (file_d->fd == fd)
               {
                 f->eax = inode_get_inumber (file_d->file->inode);
                 break; 
               }
            } 
          break;
        }
 
      default : 
        thread_current ()->exit_status = -1;
        thread_exit ();
   }
}                           

static void
exit_on_badarg (uint32_t *sp, int no_of_args)
{
  if (sp + no_of_args >= PHYS_BASE)
   {
     thread_current ()->exit_status = -1;
     thread_exit ();
   }
}
  
static bool
valid (char *uaddr)
{
  int ch;
  int i=0;
  while ((ch = get_user (uaddr + i++)) != '\0')
    {
      if (ch == -1)
         return false;
    }
  return true;
}

static bool
strcopy (uint8_t *uaddr, uint8_t *kaddr, uint32_t size)
{
  int i;
  for (i = 0; i < size; i++)
      if (!put_user (uaddr + i, *(kaddr +i))) 
         return false;
  return true;
}
                       
/* Reads a byte at user virtual address UADDR.
   Returns the byte value if successful. */
static int
get_user (const uint8_t *uaddr)
{
  if (uaddr >= PHYS_BASE)
     return -1;
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
       : "=&a" (result) : "m" (*uaddr));
  return result;
}


/* Writes BYTE to user address UDST. 
   Returns true if successful, false if a segfault occurred. */
static bool
put_user (uint8_t *udst, uint8_t byte)
{
  if (udst >= PHYS_BASE)
     return false;
  int error_code;
  asm ("movl $1f, %0; movb %b2, %1; 1:"
       : "=&a" (error_code), "=m" (*udst) : "r" (byte));
  return error_code != -1;
}
