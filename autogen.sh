#!/bin/sh

echo "Generating configuration files..."
echo

autoconf
autoheader
# evil
rm -rf autom4te.cache
./configure "$@"
