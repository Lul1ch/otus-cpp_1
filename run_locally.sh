#!/bin/bash
#!/usr/bin/env bash
set -euo pipefail

BINARY_HOME=./bin
INPUT_HOME=./input
INPUT=${INPUT_HOME}/AB_NYC_2019.csv

if [[ ! -f "${INPUT}" ]]; then
    echo "Input file not found: ${INPUT}" >&2
    exit 1
fi

echo "Mean price:"
cat "${INPUT}" \
    | "${BINARY_HOME}/mean_mapper" \
    | sort -k1,1 \
    | "${BINARY_HOME}/mean_reducer"

echo "Price variance:"
cat "${INPUT}" \
    | "${BINARY_HOME}/variance_mapper" \
    | sort -k1,1 \
    | "${BINARY_HOME}/variance_reducer"