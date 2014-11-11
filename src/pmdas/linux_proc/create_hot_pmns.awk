# Copy all the metrics that are indom PID based
# Add predicate and total, reuse control from proc
#
# states
#  0		nothing interesting is happening, looking for proc { ... }
#  1		inside proc { ... }; want to copy all lines except
#		"nprocs" metric that does not come from here for the
#		hotproc PMDA
#  2		looking for proc.foo { ... }
#  3		inside proc.foo { ... }; want to copy all lines
#  4		skip this proc.*
#
BEGIN	{ print "hotproc {"
	  state = 0
	}

state == 0 && /^proc /	{ state = 1; next }

state == 1 && /runq/	{ next }

state == 1 && /^}/	{ print "    total"; print "    predicate"; print; print ""; state = 2; next }

state == 1		{ print }

state == 2 && /^proc\.control/	{ state=4 ; next }
state == 2 && /^proc\.runq/	{ state=4 ; next }

state == 2 && /^proc\./	{ state = 3 }

state == 3 && /^}/	{ print; print ""; state = 2; next }

state == 3		{ print }

state == 4 && /^}/	{ state = 2 ; next }

state == 4		{ next }
