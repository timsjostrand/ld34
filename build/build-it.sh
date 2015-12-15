#!/bin/bash -e
tmux send -t dev 'cd /Users/tim/Code/ld34/build/unix/Debug; make run_ld34;
'
