#! /bin/sh

queue="$1"
case "$queue" in
hold)
	cat <<EOF
                                         T  5 10 20 40 80 160 320 640 1280 1280+
                                  TOTAL 10  9  8  7  6  5   4   3   2    1     0
EOF
	;;

active)
	cat <<EOF
                                         T  5 10 20 40 80 160 320 640 1280 1280+
                                  TOTAL  0  1  0  1  0  1   0   1   0    1    10
EOF
	;;
incoming)
	cat <<EOF
                                         T  5 10 20 40 80 160 320 640 1280 1280+
                                  TOTAL  9  0  1  2  3  4   5   6   7    8     9
EOF
	;;
deferred)
	cat <<EOF
                                         T  5 10 20 40 80 160 320 640 1280 1280+
                                  TOTAL 10  9  8  7  6  5   4   3   2    1     0
EOF
	;;
maildrop)
	cat <<EOF
                                         T  5 10 20 40 80 160 320 640 1280 1280+
                                  TOTAL  0  1  2  3  4  5   6   7   8    9    10
EOF
	;;
*)
 	echo "Unrecognised queue name: $queue" 1>&2
esac
