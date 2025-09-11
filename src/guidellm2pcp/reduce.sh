#!/bin/sh
# cull very long strings from input data

input="$1"
if [ -z "$input" ]; then
    echo "No file name provided"
    exit 1
fi
output="small-$input"

jq < "$input" | sed -e '/"output":/d' -e '/"prompt":/d' > "$output"
if [ $? -ne 0 ]; then
    echo "Failed to create $output from $input"
    exit 1
fi

echo "Created $output"
