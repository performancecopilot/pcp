#!/bin/bash
# SPDX-License-Identifier: BSD-2-Clause

set -euo pipefail

if [ "${V:-0}" -eq 1 ]; then
	set -x
fi

TIMEOUT=10

awk=${AWK:-awk}
readelf=${READELF:-readelf}
bpftrace=${BPFTRACE:-bpftrace}

OUTPUT=$1
TEST=$2
TEST_BIN="$1/$2"
TEST_LIB="$1/lib$2.so"

TEST_USDTS_SPEC=$TEST_BIN.exp_usdts.txt
TEST_USDTS_OUT=$TEST_BIN.act_usdts.txt
TEST_PRE_SPEC=$TEST_BIN.exp_pre_out.txt
TEST_PRE_OUT=$TEST_BIN.act_pre_out.txt
TEST_BTSCRIPT=$TEST_BIN.bt
TEST_BTOUT_SPEC=$TEST_BIN.exp_bt_out.txt
TEST_BTOUT_RAW=$TEST_BIN.act_bt_out_raw.txt
TEST_BTOUT=$TEST_BIN.act_bt_out.txt
TEST_OUT_SPEC=$TEST_BIN.exp_out.txt
TEST_OUT=$TEST_BIN.act_out.txt

$TEST_BIN -U > $TEST_USDTS_SPEC
if [ "${SHARED:-0}" -eq 1 ] && [ -e "$TEST_LIB" ]; then
	# append lib's USDT notes to USDTs from executable
	$readelf -n $TEST_BIN $TEST_LIB 2>/dev/null | $awk -f fetch-usdts.awk > $TEST_USDTS_OUT
else
	$readelf -n $TEST_BIN 2>/dev/null | $awk -f fetch-usdts.awk > $TEST_USDTS_OUT
fi
if ! $awk -f check-match.awk $TEST_USDTS_SPEC $TEST_USDTS_OUT; then
	echo "USDT SPECS MISMATCH:"
	echo "EXPECTED:"
	cat $TEST_USDTS_SPEC
	echo "ACTUAL:"
	cat $TEST_USDTS_OUT
	exit 1
fi

$TEST_BIN -t > $TEST_PRE_SPEC
$TEST_BIN > $TEST_PRE_OUT
if ! $awk -f check-match.awk $TEST_PRE_SPEC $TEST_PRE_OUT; then
	echo "TEST PRE-ATTACH OUTPUT MISMATCH:"
	echo "EXPECTED:"
	cat $TEST_PRE_SPEC
	echo "ACTUAL:"
	cat $TEST_PRE_OUT
	exit 1
fi

if ! $TEST_BIN -b | $awk -v OUTPUT=$OUTPUT -v TEST=$TEST -f prepare-bt-script.awk > $TEST_BTSCRIPT; then
	echo "FAILED TO GENERATE BPFTRACE SCRIPT:"
	cat $TEST_BTSCRIPT;
	exit 1
fi
$TEST_BIN -B > $TEST_BTOUT_SPEC

if [ -s "$TEST_BTSCRIPT" ]; then
	# start attaching bpftrace
	setsid sudo $bpftrace ${V:+-v} -B none "$TEST_BTSCRIPT" >"$TEST_BTOUT_RAW" 2>&1 &
	bt_pid=$!
	bt_pgid="$(ps -opgid= "$bt_pid" | tr -d ' ')"

	dump_bt_output() {
		echo "BPFTRACE SCRIPT:"
		cat "$TEST_BTSCRIPT"
		echo "BPFTRACE OUTPUT:"
		cat "$TEST_BTOUT_RAW"
	}

	wait_for_bpftrace() {
		local bt_start=$(date +%s)
		local stop_word="$1"
		local msg="$2"
		while true; do
			local bt_elapsed=$(( $(date +%s) - bt_start ))
			if grep -q "$stop_word" "$TEST_BTOUT_RAW"; then
				break
			elif [ "$bt_elapsed" -ge "$TIMEOUT" ]; then
				sudo kill -KILL -$bt_pgid 2>/dev/null
				echo "BPFTRACE ${msg} TIMEOUT!"
				dump_bt_output
				exit 1
			elif ! kill -s 0 "$bt_pid"; then
				echo "BPFTRACE ${msg} FAILURE!"
				dump_bt_output
				exit 1
			else
				sleep 0.2
			fi
		done
	}

	# wait for bpftrace to finish attachment
	wait_for_bpftrace "STARTED!" "STARTUP"

	# get test output while bpftrace is attached
	$TEST_BIN &>"$TEST_OUT"

	# wait for bpftrace to terminate
	wait_for_bpftrace "DONE!" "RUNNING"

	$awk '/STARTED!/ {flag=1; next} /DONE!/ {flag=0} flag' $TEST_BTOUT_RAW > $TEST_BTOUT
	if ! $awk -f check-match.awk $TEST_BTOUT_SPEC $TEST_BTOUT; then
		echo "BPFTRACE OUTPUT MISMTACH:"
		echo "BPFTRACE SCRIPT:"
		cat "$TEST_BTSCRIPT"
		echo "EXPECTED OUTPUT:"
		cat "$TEST_BTOUT_SPEC"
		echo "ACTUAL OUTPUT:"
		cat "$TEST_BTOUT"
		exit 1
	fi

	$TEST_BIN -T > $TEST_OUT_SPEC
	if ! $awk -f check-match.awk $TEST_OUT_SPEC $TEST_OUT; then
		echo "TEST ATTACHED OUTPUT MISMATCH:"
		echo "EXPECTED:"
		cat $TEST_OUT_SPEC
		echo "ACTUAL:"
		cat $TEST_OUT
		exit 1
	fi
fi
