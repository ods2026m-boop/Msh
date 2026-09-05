#!/bin/bash
# Integration test for pipelines

MSH=./msh

# Create a temporary file with some content
tmpfile=$(mktemp)
echo -e "apple\nbanana\ncherry" > "$tmpfile"

# Test simple pipeline: cat file | grep banana
out=$($MSH -c "cat $tmpfile | grep banana" < /dev/null)
if [ "$out" != "banana" ]; then
    echo "FAIL: pipeline cat | grep -> $out"
    rm -f "$tmpfile"
    exit 1
fi

# Test pipeline with two pipes
out=$($MSH -c "cat $tmpfile | grep a | grep p" < /dev/null)
if [ "$out" != "apple" ]; then
    echo "FAIL: pipeline cat | grep a | grep p -> $out"
    rm -f "$tmpfile"
    exit 1
fi

rm -f "$tmpfile"
echo "All pipeline tests passed"
exit 0
