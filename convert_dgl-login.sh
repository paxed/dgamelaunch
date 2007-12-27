#!/bin/sh
#
# This script converts the flat-text file password file
# to the new sqlite3 database.
#

DBFILE="dgamelaunch.db"

sqlite3 $DBFILE "create table dglusers (id integer primary key, username text, email text, env text, password text, flags integer);"

for x in `cat dgl-login`; do
    username="`echo $x | cut -d':' -f1`";
    email="`echo $x | cut -d':' -f2`";
    password="`echo $x | cut -d':' -f3`";
    env="`echo $x | cut -d':' -f4`";
    flags="0";

    cmdstring="insert into dglusers (username, email, env, password, flags) values ('"$username"', '"$email"', '"$env"', '"$password"', "$flags")"
    sqlite3 $DBFILE "$cmdstring"
done;

