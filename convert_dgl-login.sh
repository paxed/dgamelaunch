#!/bin/sh
#
# This script converts the flat-text password file
# to the new sqlite3 database.
#

DBFILE="dgamelaunch.db"

if [-e "$DBFILE"]; then
    echo "$DBFILE already exists.";
    exit;
fi

sqlite3 "$DBFILE" "create table dglusers (id integer primary key, username text, email text, env text, password text, flags integer);"

cat dgl-login | sed -e "s/'/''/g" -e "s/^\([^:]*\):\([^:]*\):\([^:]*\):/insert into dglusers (username, email, password, env, flags) values ('\1', '\2', '\3', '', 0); /g" > dgl-login.tmp

sqlite3 "$DBFILE" ".read dgl-login.tmp"
