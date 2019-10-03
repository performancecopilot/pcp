#! /bin/sh

echo '# HELP script_failed Simple gauge metric with one instance'
echo '# Type script_failed gauge'
echo 'script_failed{abc="0"} 456'
exit 1
