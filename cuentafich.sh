#!/bin/bash

total=$(find . -maxdepth 1 -type f -name "$1*" | wc -l)

echo "Numero de ficheros encontrado : $total"
if [ "$total" -gt 0 ]; then
    exit 0
else
    exit 1
fi