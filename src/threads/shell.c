#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <devices/input.h>
#ifdef USERPROG
#include <userprog/process.h>
#endif
#include "tests/threads/tests.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "filesys/filesys.h"

static void read_line (char line[], size_t);
static bool backspace (char **pos, char line[]);

extern int userprog;
extern bool thread_mlfqs;

int
shell_main (void)
{
  printf ("Shell starting...\n");
  for (;;) 
    {
      char command[80];

      /* Read command. */
      printf ("$> ");
      read_line (command, sizeof command);
      
      /* Execute command. */
      if (!strcmp (command, "exit"))
        break;
      else if (!memcmp (command, "run ", 4))
       {
	 userprog = 0;
         if (!memcmp (command + 4, "mlfqs", 4))
            thread_mlfqs = true;
         run_test (command + 4);
         thread_mlfqs = false;
	 userprog = 1;
         thread_set_priority (PRI_DEFAULT);
       }
      else if (!memcmp (command, "cd ", 3)) 
        chdir (command + 3);
      else if (command[0] == '\0') 
        {
          /* Empty command. */
        }
      else
        {
#ifdef USERPROG
          int pid = process_execute (command);
          if (pid != -1)
           {
             int status = process_wait (pid);
             printf ("%s: exit(%d)\n", command, status);
           }
          else
            printf ("exec failed\n");
#endif
        }
    }

  printf ("Shell exiting.");
  return 0;
}

/* Reads a line of input from the user into LINE, which has room
   for SIZE bytes.  Handles backspace and Ctrl+U in the ways
   expected by Unix users.  On return, LINE will always be
   null-terminated and will not end in a new-line character. */
static void
read_line (char line[], size_t size) 
{
  char *pos = line;
  for (;;)
    {
      char c;
      c = input_getc ();

      switch (c) 
        {
        case '\r':
          *pos = '\0';
          printf ("\n");
          return;

        case '\b':
          backspace (&pos, line);
          break;

        case ('U' - 'A') + 1:       /* Ctrl+U. */
          while (backspace (&pos, line))
            continue;
          break;

        default:
          /* Add character to line. */
          if (pos < line + size - 1) 
            {
              printf ("%c",c);
              *pos++ = c;
            }
          break;
        }
    }
}

/* If *POS is past the beginning of LINE, backs up one character
   position.  Returns true if successful, false if nothing was
   done. */
static bool
backspace (char **pos, char line[]) 
{
  if (*pos > line)
    {
      /* Back up cursor, overwrite character, back up
         again. */
      printf ("\b \b");
      (*pos)--;
      return true;
    }
  else
    return false;
}
