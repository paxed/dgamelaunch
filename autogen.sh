#!/bin/sh

echo "Generating configuration files..."
echo

autoreconf
# evil
rm -rf autom4te.cache
./configure "$@"
