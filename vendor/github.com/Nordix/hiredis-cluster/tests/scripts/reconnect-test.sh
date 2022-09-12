#!/bin/sh

# Usage: $0 /path/to/clusterclient-binary

clientprog=${1:-./clusterclient_reconnect_async}
testname=reconnect-test

# Sync process just waiting for server to be ready to accept connection.
perl -we 'use sigtrap "handler", sub{exit}, "CONT"; sleep 1; die "timeout"' &
syncpid=$!

# Start simulated server
timeout 5s ./simulated-redis.pl -p 7400 -d --sigcont $syncpid <<'EOF' &
EXPECT CONNECT
EXPECT ["CLUSTER", "SLOTS"]
SEND -ERR This instance has cluster support disabled
EXPECT CLOSE
EXPECT CONNECT
EXPECT ["SET", "foo", "bar"]
SEND +OK
EXPECT ["GET", "foo"]
SEND "bar"
CLOSE
EXPECT CONNECT
EXPECT ["CLUSTER", "SLOTS"]
SEND -ERR This instance has cluster support disabled
EXPECT CLOSE
EXPECT CONNECT
EXPECT ["GET", "foo"]
SEND "bar"
EXPECT CLOSE
EOF
server=$!

# Wait until server is ready to accept client connection
wait $syncpid;

# Run client
timeout 3s "$clientprog" 127.0.0.1:7400 > "$testname.out" <<'EOF'
SET foo bar
GET foo
GET foo
GET foo
EOF
clientexit=$?

# Wait for server to exit
wait $server; serverexit=$?

# Check exit statuses
if [ $serverexit -ne 0 ]; then
    echo "Simulated server exited with status $serverexit"
    exit $serverexit
fi
if [ $clientexit -ne 0 ]; then
    echo "$clientprog exited with status $clientexit"
    exit $clientexit
fi

# Check the output from clusterclient
printf '[no cluster]\nOK\nbar\n[reconnect]\n[no cluster]\nbar\n' | cmp "$testname.out" - || exit 99

# Clean up
rm "$testname.out"
