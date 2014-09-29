# Insert the necessary hotproc label into the root group
#
# states
#  0		copy input, looking for root { ... }
#  1		inside root { ... }; want to copy all lines 
#		and add hotproc
#  2		just copy input to the end
#
BEGIN	{ 
	  state = 0
	}

state == 0 && /^root /	{ print ; state = 1; next }

state == 0		{ print }

state == 1 && /^}/	{ print "    hotproc"; print; state = 2; next }

state == 1		{ print }

state == 2		{ print }
