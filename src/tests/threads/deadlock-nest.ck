# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(deadlock-nest) begin
(deadlock-nest) main tries to acquire lock 0.
(deadlock-nest) main acquired lock 0.
(deadlock-nest) child 1 tries to acquire lock 1.
(deadlock-nest) child 1 acquired lock 1.
(deadlock-nest) child 2 tries to acquire lock 2.
(deadlock-nest) child 2 acquired lock 2.
(deadlock-nest) child 2 tries to acquire lock 0.
(deadlock-nest) main tries to acquire lock 1.
(deadlock-nest) child 1 tries to acquire lock 2.
(deadlock-nest) child 1 could not acquire lock 2.
(deadlock-nest) child 1 released lock 1.
(deadlock-nest) main acquired lock 1.
(deadlock-nest) main released lock 1.
(deadlock-nest) main released lock 0.
(deadlock-nest) child 2 acquired lock 0.
(deadlock-nest) child 2 released lock 0.
(deadlock-nest) child 2 released lock 2.
(deadlock-nest) child 2 ends
(deadlock-nest) child 1 ends
(deadlock-nest) main ends
(deadlock-nest) end
EOF
pass;
