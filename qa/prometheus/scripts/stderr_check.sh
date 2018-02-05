#! /bin/sh

echo '# HELP stderr_check Simple gauge metric with one instance'
echo '# Type stderr_check gauge'
echo 'stderr_check{abc="0"} 456'
>&2 echo stderr output should end up in the PMDA log file
exit 0
