#!/bin/bash
# Integration test for builtins

MSH=./msh

# Test echo
out=$($MSH -c 'echo hello' < /dev/null)
if [ "$out" != "hello" ]; then
    echo "FAIL: echo hello -> $out"
    exit 1
fi

# Test echo with double quotes and variable expansion
out=$($MSH -c 'X=world; echo "hello $X"' < /dev/null)
if [ "$out" != "hello world" ]; then
    echo "FAIL: echo \"hello $X\" -> $out"
    exit 1
fi

# Test alias
out=$($MSH -c 'alias ll="echo aliased"; ll' < /dev/null)
if [ "$out" != "aliased" ]; then
    echo "FAIL: alias ll -> $out"
    exit 1
fi

# Test pwd
out=$($MSH -c 'pwd' < /dev/null)
# We just check that it runs without error; output may vary
if [ $? -ne 0 ]; then
    echo "FAIL: pwd failed"
    exit 1
fi

echo "All builtin tests passed"
exit 0
