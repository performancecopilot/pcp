. /etc/pcp.env

export PCP_STDERR=""
export PATH=.:$PATH

# get offset into an archive relative to the first pmResult
# past the preamble
#
# Usage: _arch_start archive [offset]
#
_arch_start()
{
    pmdumplog -z $1 \
    | $PCP_AWK_PROG '
/^[0-9][0-9]:[0-9][0-9]:/	{ if ($3 ~ /pmcd.pmlogger.host/) next
				  split($1, t, ":")
				  t[3] += '"${2-0}"'
				  while (t[3] < 0) {
				    t[3] += 60
				    t[2]--
				  }
				  while (t[3] > 60) {
				    t[3] -= 60
				    t[2]++
				  }
				  while (t[2] < 0) {
				    t[2] += 60
				    t[1]--
				  }
				  while (t[2] > 60) {
				    t[2] -= 60
				    t[1]++
				  }
				  while (t[1] < 0)
				    t[1] += 24
				  while (t[1] > 23)
				    t[1] -= 24
				  printf "@%02d:%02d:%06.3f",t[1],t[2],t[3]
				  exit
				}'
}
