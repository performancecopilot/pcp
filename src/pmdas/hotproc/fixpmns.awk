# states
#  0		nothing interesting is happening, looking for proc { ... }
#  1		inside proc { ... }; want to copy all lines except
#		"nprocs" metric that does not come from here for the
#		hotproc PMDA
#  2		looking for proc.foo { ... }
#  3		inside proc.foo { ... }; want to copy all lines
#
BEGIN	{ print "/*"
	  print " * Hotproc Performance Metric Domain (PMD) Identifiers"
	  print " * (generated from pmns for hotproc and proc metrics)"
	  print " */"
	  print ""
	  print "hotproc {"
	  print "    nprocs	HOTPROC:100:0"
	  print "    cpuburn	HOTPROC:102:0"
	  print "    total"
	  print "    predicate"
	  print "    control"
	  state = 0
	}

state == 0 && /^proc /	{ state = 1; next }

state == 1 && /nprocs/	{ next }

state == 1 && /^}/	{ print; print ""; state = 2; next }

state == 1		{ print }

state == 2 && /^proc\./	{ state = 3 }

state == 3 && /^}/	{ print; print ""; state = 2; next }

state == 3		{ print }
