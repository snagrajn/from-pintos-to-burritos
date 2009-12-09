# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(deadlock-simple) begin
(deadlock-simple) main tries to acquire lock 0.
(deadlock-simple) main acquired lock 0.
(deadlock-simple) child tries to acquire lock 1.
(deadlock-simple) child acquired lock 1.
(deadlock-simple) child tries to acquire lock 0.
(deadlock-simple) main tries to acquire lock 1.
(deadlock-simple) main could not acquire lock 1.
(deadlock-simple) main released lock 0.
(deadlock-simple) child acquired lock 0.
(deadlock-simple) child released lock 0.
(deadlock-simple) child released lock 1.
(deadlock-simple) child ends
(deadlock-simple) main ends
(deadlock-simple) end
EOF
pass;
