#!/bin/sh

echo "Generating configuration files..."
echo

autoconf
autoheader
rm -rf autom4te.cache # evil
./configure "$@"
