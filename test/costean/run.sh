#!/bin/bash
# Costean OS host unit tests: pool split + failover, scheduler, governor/auto-tune, long history.
# usage: test/costean/run.sh   (from the repository root). Needs g++ with AddressSanitizer.
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd); W=${TMPDIR:-/tmp}/costean-unit; mkdir -p $W
F="-std=c++17 -g -O1 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer"
S=$ROOT/main/costean; T=$ROOT/test/costean; rc=0
g++ $F $T/test_pool_split.cpp $S/pool_split.cpp -o $W/t_split
g++ $F $T/test_scheduler.cpp $S/scheduler.cpp $S/tuning.cpp -o $W/t_sched
g++ $F $T/test_tuning.cpp $S/tuning.cpp $S/rollup.cpp -o $W/t_tune
for t in t_split t_sched t_tune; do $W/$t || rc=1; done
exit $rc
