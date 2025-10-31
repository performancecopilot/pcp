# SPDX-License-Identifier: BSD-2-Clause

function reset_entry()
{
	entry = "";
	grp  = "???";
	name = "???";
	base = "???";
	sema = "???";
	args = "???";
	argn = 0;
}

function print_entry()
{
	base_key = filename ":" base;
	if (base != 0 && !(base_key in basemap)) {
		basemap[base_key] = ++base_cnt;
	}

	sema_key = filename ":" sema;
	if (sema != 0 && !(sema_key in semamap)) {
		semamap[sema_key] = ++sema_cnt;
	}

	base_stub = (base == 0) ? "0" : sprintf("BASE%d", basemap[base_key]);
	sema_stub = (sema == 0) ? "0" : sprintf("SEMA%d", semamap[sema_key]);

	printf "%s:%s base=%s sema=%s argn=%d args=%s.\n",
	       grp, name, base_stub, sema_stub, argn, args;
}

BEGIN {
	reset_entry();
	filename = "";
}

#  stapsdt              0x0000003b       NT_STAPSDT (SystemTap probe descriptors)
#    Provider: test
#    Name: name4
#    Location: 0x0000000000401198, Base: 0x0000000000402043, Semaphore: 0x0000000000000000
#    Arguments: -4@$1 -4@$2 -4@$3 -4@$4

/File:/ {
	if (entry != "") {
		print_entry();
		reset_entry();
	}
	filename = $2;
}

/\sstapsdt\s/ {
	if (entry != "") {
		print_entry();
		reset_entry();
	}
	entry = $0;
	next
}

/Provider:/ { grp = $2; }
/Name:/ { name = $2; }
/Location:/ { base = strtonum($4); sema = strtonum($6); }
/Arguments:/ {
	arg_str = substr($0, index($0, "Arguments: ") + 11);
	# Count arguments by looking for patterns like "-4@" that start each argument
	argn = gsub(/-?[0-9]+@/, "&", arg_str);
	args = (argn > 0) ? arg_str : "";
}

END {
	if (entry != "")
	{
		print_entry();
	}
}
