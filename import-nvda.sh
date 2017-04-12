#!/bin/sh

# error out upon command error
set -e

NVDADIR=$1

# import all symbols.dic
test -d "$NVDADIR"/source/locale
for f in "$NVDADIR"/source/locale/*/symbols.dic; do
  dest=$(echo "$f" | sed 's|^.*/locale/|locale/|')
  cp -v "$f" "$dest"
done
