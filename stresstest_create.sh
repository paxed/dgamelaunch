#!/bin/sh
# stress-test script for creation of new accounts
# current directory must contain the dgamelaunch to be used
# this thing eats pseudo-terminals like hell!

# Be sure to change these variables
conffile="test2.conf"
chrootdir="chroot-2"
hackdir="/usr/local/lib/nethack"
nameprefix="testcreat"
jailuser="dgl"
jailgroup="dgl"
max=40

# clean up previous cruft
#rm -f "$chrootdir"/dgldir/inprogress/*
#rm -f "$chrootdir$hackdir"/save/*[0-9]test[0-9]*

echo -n "Starting processes:"
export conffile i nameprefix delay1 delay2 email
i=1
pidlist=''
while [ $i -le $max ]; do
	email="$nameprefix$i@nowhere.nowhere"
	case "$i" in
	*[125])	delay1=0.1 delay2=0.1 ;;
	*[368])	delay1=0.1 delay2=0.1 ;;
	*[470]) delay1=0.1 delay2=0.1 ;;
	*9) delay1=0.1 delay2=0.1 email='' ;;
	esac
	xterm -e sh -c '{ sleep 1; echo "r$nameprefix$i"; echo aa; echo aa; sleep $delay1; echo "$email"; sleep $delay2; echo -n py i; cat; } | { ./dgamelaunch -f "$conffile"; echo $?; } '&
	echo -n " $!"
	pidlist="$pidlist $!"
	[ $i = 5 ] && sleep 0.1
	[ $i = 15 ] && sleep 0.3
	[ $i = 25 ] && sleep 1
	[ $i = 31 ] && sleep 4
	i=$(($i+1))
done
echo
sleep 10
nums=$(sed -n -e "s/^$nameprefix\([0-9]*\):.*:.*:.*/\1/p" $chrootdir/dgl-login |
	sort -n | xargs)
allnums=$(jot $max | grep -v 9\$ | xargs)
if [ "$nums" = "$allnums" ]; then
	echo "Complete list"
	rc=0
else
	echo "ERROR: Incomplete list!"
	echo "$nums"
	rc=1
fi
sed -e "/^$nameprefix\([0-9]*\):.*:.*:.*/d" $chrootdir/dgl-login > $chrootdir/dgl-login.new
mv $chrootdir/dgl-login.new $chrootdir/dgl-login
chown $jailuser:$jailgroup $chrootdir/dgl-login
echo -n "Press return to remove all xterms: "
read x
kill $pidlist 2>/dev/null
exit $rc
