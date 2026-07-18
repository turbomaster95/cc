#!/bin/sh

if [ -z "$1" ]; then
    echo "Error: Please specify the compiler binary."
    echo "Usage: $0 ./c99x86"
    exit 1
fi

COMPILER="$1"

if [ ! -x "$COMPILER" ]; then
    echo "Error: Compiler binary '$COMPILER' not found or not executable."
    exit 1
fi

for test_file in tests/*.c; do
    [ -e "$test_file" ] || continue
    echo "==== Running $test_file ===="

    "$COMPILER" "$test_file"

    STATUS=$?
    if [ $STATUS -ne 0 ]; then
        echo "❌ Failed with exit code $STATUS"
    else
        echo "✅ Passed"
    fi
    echo ""
done

