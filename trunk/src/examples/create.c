/* create.c

   Creates files specified on command line. */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <syscall.h>

static int read_line (char line[]);
static bool backspace (char **pos, char line[]);

int
main (int argc, char *argv[]) 
{
  bool success = true;
  int i;

  char buffer[4096];
  printf ("Please enter your text here:\n");
  int size = read_line (buffer);
  int fd;
  
  for (i = 1; i < argc; i++)
    {
      if (!create (argv[i], size)) 
       {
         printf ("%s: create failed\n", argv[i]);
         success = false; 
       }
      else if ((fd = open (argv[i])) == -1)
       {
         printf ("%s: write failed\n", argv[i]);
         success = false; 
       }
      else if (write (fd, buffer, size) == -1)
       {
         printf ("%s: write failed\n", argv[i]);
         success = false; 
       }
    }
  return success ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* Reads a line of input from the user into LINE, which has room
   for SIZE bytes.  Handles backspace and Ctrl+U in the ways
   expected by Unix users.  On return, LINE will always be
   null-terminated and will not end in a new-line character. */
static int
read_line (char line[]) 
{
  char *pos = line;
  for (;;)
    {
      char c;
      read (STDIN_FILENO, &c, 1);

      switch (c) 
        {
        case '\r':
          *pos++ = '\n';
          putchar ('\n');
          break;

        case '\b':
          backspace (&pos, line);
          break;

        case ('D' - 'A') + 1:       /* Ctrl+D. */
          *pos = '\0';
          putchar ('\n'); 
	  return strlen (line);

        case ('U' - 'A') + 1:       /* Ctrl+U. */
          while (backspace (&pos, line))
            continue;
          break;

        default:
          /* Add character to line. */
          if (pos < line + 255)
           {
             putchar (c);
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
