/* Executes and waits for multiple child processes. */

#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void) 
{
  printf ("wait (exec (\"child-simple\")) : %d\n",wait (exec ("child-simple")));
  printf ("wait (exec (\"child-simple\")) : %d\n",wait (exec ("child-simple")));
  printf ("wait (exec (\"child-simple\")) : %d\n",wait (exec ("child-simple")));
  printf ("wait (exec (\"child-simple\")) : %d\n",wait (exec ("child-simple")));
}
