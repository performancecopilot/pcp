# SPDX-License-Identifier: BSD-2-Clause

# Example inputs:
# group:name { some %s fmt %d spec %d -> str(arg0), (int)arg1, arg2 - 10 }
{
	if (!has_contents)
		printf("BEGIN { printf(\"STARTED!\\n\"); }\n");
	has_contents = 1;

	# Split between { and }, extract insides of { }
	split($0, parts, /[{}]/)

	probe_spec = parts[1]
	gsub(/^[ \t]+|[ \t]+$/, "", probe_spec) # trim trailing whitespaces

	split(probe_spec, probe, ":")
	if (length(probe) == 3 && probe[1] == "lib") {	# lib:<group>:<name>
		path = OUTPUT "/lib" TEST ".so";
		group = probe[2];
		name = probe[3];
	} else if (length(probe) == 2) {		# <group>:<name>
		path = OUTPUT "/" TEST;
		group = probe[1];
		name = probe[2];
	} else {
		printf("UNEXPECTED target binary specification in '%s'!\n", parts[1]);
		exit(1);
	}

	stmt_spec = parts[2]
	gsub(/^[ \t]+|[ \t]+$/, "", stmt_spec) # trim trailing whitespaces

	# Split by (optional) ->, trim spaces, extract fmt spec and args
	split(stmt_spec, stmt_parts, "->");
	fmt = stmt_parts[1];
	args = stmt_parts[2];
	gsub(/^[ \t]+|[ \t]+$/, "", fmt) # trim trailing whitespaces
	gsub(/^[ \t]+|[ \t]+$/, "", args) # trim trailin whitespaces

	# Emit corresponding bpftrace probe spec:
	# U:./test:group:name { printf("%s: some %s fmt %d spec %d\n", probe, str(arg0), (int)arg1, arg2 - 10); }
	printf("U:%s:%s:%s { printf(\"%s%s:%s: %s\\n\"%s); }\n",
	       path, group, name,
	       probe[1] == "lib" ? "lib:" : "", group, name,
	       fmt, args == "" ? "" : ", " args);
}

END {
	if (has_contents) {
		printf("uretprobe:%s:main { exit(); }\n", OUTPUT "/" TEST);
		printf("END { printf(\"DONE!\\n\"); }\n");
	}
}
