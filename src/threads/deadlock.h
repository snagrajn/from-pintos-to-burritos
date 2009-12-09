#include <stdbool.h>
#include "threads/thread.h"

bool someone_not_visited (int *, int);
bool detect_cycles (int **, int);
bool detect_deadlocks (struct thread *, struct thread *);
int get_id (struct thread *);
