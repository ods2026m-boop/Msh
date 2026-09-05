#!/bin/bash
# Integration test for redirection

MSH=./msh

# Test output redirection
outfile=$(mktemp)
$MSH -c "echo hello > $outfile" < /dev/null
if [ ! -f "$outfile" ] || [ "$(cat $outfile)" != "hello" ]; then
    echo "FAIL: output redirection"
    rm -f "$outfile"
    exit 1
fi
rm -f "$outfile"

# Test input redirection
infile=$(mktemp)
echo "hello world" > "$infile"
out=$($MSH -c "cat < $infile" < /dev/null)
if [ "$out" != "hello world" ]; then
    echo "FAIL: input redirection"
    rm -f "$infile"
    exit 1
fi
rm -f "$infile"

# Test redirection with variable
outfile=$(mktemp)
$MSH -c "X=hello; echo \$X > $outfile" < /dev/null
if [ ! -f "$outfile" ] || [ "$(cat $outfile)" != "hello" ]; then
    echo "FAIL: redirection with variable"
    rm -f "$outfile"
    exit 1
fi
rm -f "$outfile"

echo "All redirection tests passed"
exit 0
