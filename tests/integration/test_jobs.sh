#!/bin/bash
# Integration test for job control

MSH=./msh

# Test background job: sleep 1 & should return immediately
# We'll use a command that prints something to verify it runs
outfile=$(mktemp)
$MSH -c "echo start > $outfile & sleep 0.1; echo end >> $outfile" < /dev/null
# Wait a bit for background job to finish
sleep 0.2
out=$(cat $outfile)
expected="start
end"
if [ "$out" != "$expected" ]; then
    echo "FAIL: background job output mismatch"
    echo "Got: $out"
    echo "Expected: $expected"
    rm -f "$outfile"
    exit 1
fi
rm -f "$outfile"

# Test that the shell can list jobs (jobs builtin)
out=$($MSH -c 'jobs' < /dev/null)
# We just check that it runs without error
if [ $? -ne 0 ]; then
    echo "FAIL: jobs builtin failed"
    exit 1
fi

echo "All job tests passed"
exit 0
