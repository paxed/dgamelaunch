#!/bin/sh
# stress-test script
# requires users test1...test${max} to be defined in /dgl-login, /dgldir/ttyrec
# and /dgldir/rcfiles.
# current directory must contain the dgamelaunch to be used

# Be sure to change these variables
conffile="test2.conf"
chrootdir="chroot-2"
hackdir="/usr/local/lib/nethack"
max=19

# clean up previous cruft
rm -f "$chrootdir"/dgldir/inprogress/*
rm -f "$chrootdir$hackdir"/save/*[0-9]test[0-9]*

echo -n "Starting processes:"
export conffile i
i=1
while [ $i -le $max ]; do
	xterm -e sh -c '(sleep 1; echo "ltest$i"; echo aa; echo -n py i; cat) | ./dgamelaunch -f "$conffile" '&
	echo -n " $!"
	sleep 0.1
	i=$(($i+1))
done
echo
