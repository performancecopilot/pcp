# SPDX-License-Identifier: BSD-2-Clause
#
# Validate that every USDT note's location falls inside an executable
# PT_LOAD segment of its ELF file, mirroring the lookup tracing tools
# perform (e.g., libbpf's find_elf_seg()). A location that lands in no
# segment means the note's relocation was resolved against a section the
# linker discarded (garbage-collected or deduplicated COMDAT), leaving a
# dangling note that breaks probe attachment.
#
# Input is the concatenated output of `readelf -lW <file>` and
# `readelf -n <file>` for a single ELF file:
#
#   LOAD  0x000000 0x0000000000400000 0x0000000000400000 0x0004f8 0x0004f8 R E 0x1000
#   ...
#     Provider: test
#     Name: kept
#     Location: 0x0000000000401234, Base: ..., Semaphore: ...
#
# Exits non-zero listing each dangling note.

BEGIN { nsegs = 0; }

# readelf -lW prints the flag column with a space inside it ("R E"), so it is
# split across fields; the trailing align column is always last. Reconstruct
# the flags from everything between memsz ($6) and align ($NF).
$1 == "LOAD" {
	flags = "";
	for (i = 7; i < NF; i++)
		flags = flags $i;
	if (flags ~ /E/) {
		seg_start[nsegs] = strtonum($3);
		seg_end[nsegs] = seg_start[nsegs] + strtonum($6);
		nsegs++;
	}
}

/Provider:/ { grp = $2; }
/Name:/ { name = $2; }

/Location:/ {
	loc = strtonum($2);
	for (i = 0; i < nsegs; i++) {
		if (loc >= seg_start[i] && loc < seg_end[i])
			next;
	}
	printf "%s:%s location 0x%x is in no executable segment (dangling note)\n",
	       grp, name, loc;
	bad = 1;
}

END {
	exit bad;
}
