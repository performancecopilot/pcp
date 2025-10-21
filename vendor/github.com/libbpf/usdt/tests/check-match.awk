# SPDX-License-Identifier: BSD-2-Clause

function glob_to_regex(pat)
{
	gsub(/\\/, "\\\\", pat);		# Escape backslashes
	gsub(/\./, "\\.", pat);			# . -> \. (escape dot)
	gsub(/\+/, "\\+", pat);			# + -> \+ (escape plus)
	gsub(/\$/, "\\$", pat);			# $ -> \$ (escape dollar sign)
	gsub(/\^/, "\\^", pat);			# ^ -> \^ (escape caret)
	gsub(/\(/, "\\(", pat);			# ( -> \(
	gsub(/\)/, "\\)", pat);			# ) -> \)
	gsub(/\[!\]/, "[^", pat);		# [!] -> [^
	gsub(/\[([^\]]*)\]/, "[\\1]", pat);	# [] -> []
	gsub(/\*/, ".*", pat);			# * -> .*
	gsub(/\?/, ".", pat);			# ? -> .
	pat = sprintf("^%s$", pat);
	return pat;
}

BEGIN {
	next_exp = 0;
	first_filename = "";
}

# gawk extension
BEGINFILE {
	if (first_filename == "")
		first_filename = FILENAME;
}

NR == FNR {
	if (FILENAME != first_filename)
		exit 0;

	# remember each non-empty line from first file (expectations)
	exp_lines[NR - 1] = $0;
	next;
}

{
	line_nr = FNR; # line number within the second file (actual outputs)
	pat = exp_lines[next_exp];
	next_pat = ((next_exp + 1) in exp_lines) ? exp_lines[next_exp + 1] : "";

	if (pat == "") {
		print "Error: no matching expected pattern for line #" line_nr;
		exit 1;
	}

	# glob to regexp conversion
	rpat = glob_to_regex(pat);
	next_rpat = next_pat == "" ? "" : glob_to_regex(next_pat);

	# '...' pattern matches non-eagerly
	if (pat == "..." && next_pat != "" && ($0 ~ next_rpat)) {
		# printf("match after ... nextpat='%s'  %s\n", next_pat, $0);
		next_exp++;
		next_exp++;
		next;
	}

	if (pat == "...") {
		# printf("match ...%s\n", $0);
		next;
	}

	if ($0 ~ rpat) {
		# printf("match exact pat='%s' act='%s'\n", pat, $0);
		next_exp++;
		next;
	}

	printf("Error: line #%d doesn't match expected pattern:\n", line_nr);
	printf("  Expected (#%d): %s\n", next_exp + 1, pat);
	printf("  Regexp (#%d):   %s\n", next_exp + 1, rpat);
	printf("  Actual (#%d):   %s\n", line_nr, $0);
	exit 1;
}

END {
	# if there are no expectations, it's a success
	if (length(exp_lines) == 0)
		exit 0;

	# if last pattern is just '...', it implicitly matches empty output
	if (next_exp == length(exp_lines) - 1 && exp_lines[next_exp] == "...")
		exit 0;

	# check that we matched all patterns
	if (next_exp < length(exp_lines)) {
		printf("Error: not all patterns matched (stopped at line #%d out of %d)\n",
		       next_exp + 1, length(exp_lines));
		exit 1;
	}
}
