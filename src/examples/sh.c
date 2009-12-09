#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <syscall.h>

static void read_line (int fd, char line[], size_t);

int
main (int argc, char *argv[])
{
  int fd = open (argv[1]);
  for (;;) 
    {
      char command[80];

      /* Read command. */
      read_line (fd, command, sizeof command);
      
      /* Execute command. */
      if (!strcmp (command, "exit"))
        break;
      else if (!memcmp (command, "cd ", 3)) 
        chdir (command + 3);
      else if (command[0] == '\0') 
        {
          /* Empty command. */
        }
      else
        {
          pid_t pid = exec (command);
          if (pid != PID_ERROR)
            wait (pid);
          else
            printf ("exec failed\n");
        }
    }

  printf ("Shell exiting.");
  return EXIT_SUCCESS;
}

/* Reads a line of input from the user into LINE, which has room
   for SIZE bytes.  Handles backspace and Ctrl+U in the ways
   expected by Unix users.  On return, LINE will always be
   null-terminated and will not end in a new-line character. */
static void
read_line (int fd, char line[], size_t size) 
{
  char *pos = line;
  for (;;)
    {
      char c;
      read (fd, &c, 1);

      switch (c) 
        {
        case '\r':
          *pos = '\0';
          return;

        default:
          /* Add character to line. */
          if (pos < line + size - 1) 
              *pos++ = c;
          break;
        }
    }
}
