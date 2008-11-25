#!/bin/sh
#
# This script converts the flat-text password file
# to the new sqlite3 database.
#


TEMPFILE="dgl-login.tmp"
FLATDB="dgl-login"
DBFILE="dgamelaunch.db"


if [ -e "$TEMPFILE" ]; then
    echo "$TEMPFILE already exists.";
    exit;
fi
if [ -e "$DBFILE" ]; then
    echo "$DBFILE already exists.";
    exit;
fi
if [ ! -e "$FLATDB" ]; then
    echo "$FLATDB does not exist.";
    exit;
fi


sqlite3 "$DBFILE" "create table dglusers (id integer primary key, username text, email text, env text, password text, flags integer);"

cat "$FLATDB" | sed -e "s/'/''/g" -e "s/^\([^:]*\):\([^:]*\):\([^:]*\):/insert into dglusers (username, email, password, env, flags) values ('\1', '\2', '\3', '', 0); /g" > "$TEMPFILE"

sqlite3 "$DBFILE" ".read $TEMPFILE"

rm -f "$TEMPFILE"
