#!/bin/sh
#
# PMID Consistency Validator
# Validates PMID consistency across pmns, metrics.c, and darwin.h
#
# Strategy: Validate PMIDs (cluster:item pairs), not names
# - pmns defines: metric_name → DARWIN:cluster:item
# - metrics.c has: PMDA_PMID(CLUSTER_NAME, item)
# - Validate: every pmns PMID exists in metrics.c, no duplicates
#
# Exit codes:
#   0 - All validations passed
#   1 - Validation failures detected
#   2 - Missing required files or parse errors

set -e

REPO_ROOT="$(cd "$(dirname "$0")/../../../.." && pwd)"
DARWIN_DIR="$REPO_ROOT/src/pmdas/darwin"

PMNS_FILE="$DARWIN_DIR/pmns"
METRICS_FILE="$DARWIN_DIR/metrics.c"
HEADER_FILE="$DARWIN_DIR/darwin.h"

# Temp files for parsing
TMPDIR="${TMPDIR:-/tmp}"
CLUSTERS_TMP="$TMPDIR/pmid_clusters.$$"
PMNS_PMIDS="$TMPDIR/pmid_pmns_pmids.$$"
METRICS_PMIDS="$TMPDIR/pmid_metrics_pmids.$$"

# Cleanup on exit
trap 'rm -f "$CLUSTERS_TMP" "$PMNS_PMIDS" "$METRICS_PMIDS"' EXIT INT TERM

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

errors=0
warnings=0

# Validate required files exist
for file in "$PMNS_FILE" "$METRICS_FILE" "$HEADER_FILE"; do
    if [ ! -f "$file" ]; then
        printf "${RED}ERROR: Required file not found: %s${NC}\n" "$file" >&2
        exit 2
    fi
done

# Step 1: Parse cluster definitions from darwin.h
sed -n '/^enum {$/,/^};$/p' "$HEADER_FILE" | \
    grep "CLUSTER_" | \
    awk '
BEGIN {
    counter = 0;
}
/CLUSTER_/ {
    line = $0;
    sub(/^[[:space:]]*CLUSTER_/, "", line);
    cluster_name = line;
    sub(/[[:space:],].*/, "", cluster_name);

    if (index($0, "=") > 0) {
        line = $0;
        sub(/.*=[[:space:]]*/, "", line);
        sub(/[^0-9].*/, "", line);
        if (length(line) > 0) {
            counter = line + 0;
        }
    }

    print cluster_name, counter;
    counter++;
}
' > "$CLUSTERS_TMP"

if [ ! -s "$CLUSTERS_TMP" ]; then
    printf "${RED}ERROR: No cluster definitions found in %s${NC}\n" "$HEADER_FILE" >&2
    exit 2
fi

# Step 2: Parse pmns file - extract PMIDs only (ignore names)
# Format: cluster:item
awk '/DARWIN:[0-9]+:[0-9]+/ {
    pmid_str = $0;
    sub(/.*DARWIN:/, "", pmid_str);
    sub(/[^0-9:].*/, "", pmid_str);
    print pmid_str;
}' "$PMNS_FILE" | sort -u > "$PMNS_PMIDS"

if [ ! -s "$PMNS_PMIDS" ]; then
    printf "${RED}ERROR: No PMIDs found in %s${NC}\n" "$PMNS_FILE" >&2
    exit 2
fi

# Step 3: Parse metrics.c - extract PMDA_PMID calls
# Format: CLUSTER_NAME item
awk '/PMDA_PMID\(CLUSTER_/ {
    gsub(/.*PMDA_PMID\(CLUSTER_/, "");
    gsub(/\).*/, "");
    gsub(/,/, " ");
    print $0;
}' "$METRICS_FILE" | \
awk '
NR == FNR {
    cluster_map[$1] = $2;
    next;
}
{
    cluster_name = $1;
    item = $2;
    if (cluster_name in cluster_map) {
        print cluster_map[cluster_name] ":" item;
    } else {
        print "ERROR: Unknown cluster " cluster_name > "/dev/stderr";
        error_count++;
    }
}
END {
    if (error_count > 0) exit 1;
}
' "$CLUSTERS_TMP" - | sort > "$METRICS_PMIDS"

if [ $? -ne 0 ]; then
    errors=$((errors + 1))
fi

if [ ! -s "$METRICS_PMIDS" ]; then
    printf "${RED}ERROR: No PMIDs found in %s${NC}\n" "$METRICS_FILE" >&2
    exit 2
fi

printf "Validating PMID consistency...\n"
printf "  PMNS unique PMIDs: %d\n" "$(wc -l < "$PMNS_PMIDS" | tr -d ' ')"
printf "  metrics.c PMIDs: %d\n" "$(wc -l < "$METRICS_PMIDS" | tr -d ' ')"
printf "  Cluster definitions: %d\n" "$(wc -l < "$CLUSTERS_TMP" | tr -d ' ')"
printf "\n"

# Validation 1: Check for PMIDs in pmns but missing from metrics.c
printf "Check 1: PMIDs in pmns exist in metrics.c\n"
MISSING_IN_METRICS=$(comm -23 "$PMNS_PMIDS" "$METRICS_PMIDS")

if [ -n "$MISSING_IN_METRICS" ]; then
    printf "${RED}ERROR: PMIDs defined in pmns but missing from metrics.c:${NC}\n"
    while read pmid; do
        # Find metric name from pmns for better error message
        metric_name=$(awk -v pmid="$pmid" '$0 ~ "DARWIN:" pmid {print $1; exit}' "$PMNS_FILE")
        printf "  DARWIN:%s" "$pmid"
        if [ -n "$metric_name" ]; then
            printf " (%s)" "$metric_name"
        fi
        printf "\n"
        errors=$((errors + 1))
    done <<EOF
$MISSING_IN_METRICS
EOF
else
    printf "${GREEN}✓ All pmns PMIDs exist in metrics.c${NC}\n"
fi
printf "\n"

# Validation 2: Check for PMIDs in metrics.c but not in pmns (suspicious)
printf "Check 2: Extra PMIDs in metrics.c not in pmns\n"
EXTRA_IN_METRICS=$(comm -13 "$PMNS_PMIDS" "$METRICS_PMIDS")

if [ -n "$EXTRA_IN_METRICS" ]; then
    printf "${YELLOW}WARNING: PMIDs in metrics.c but not defined in pmns:${NC}\n"
    while read pmid; do
        printf "  DARWIN:%s\n" "$pmid"
        warnings=$((warnings + 1))
    done <<EOF
$EXTRA_IN_METRICS
EOF
else
    printf "${GREEN}✓ No extra PMIDs in metrics.c${NC}\n"
fi
printf "\n"

# Validation 3: Check for duplicate PMIDs in metrics.c
printf "Check 3: Duplicate PMID detection\n"

# Count occurrences before sort -u
awk '/PMDA_PMID\(CLUSTER_/ {
    gsub(/.*PMDA_PMID\(CLUSTER_/, "");
    gsub(/\).*/, "");
    gsub(/,/, " ");
    print $0;
}' "$METRICS_FILE" | \
awk '
NR == FNR {
    cluster_map[$1] = $2;
    next;
}
{
    cluster_name = $1;
    item = $2;
    if (cluster_name in cluster_map) {
        pmid = cluster_map[cluster_name] ":" item;
        count[pmid]++;
        if (count[pmid] == 1) {
            first_line[pmid] = NR;
        }
    }
}
END {
    duplicates = 0;
    for (pmid in count) {
        if (count[pmid] > 1) {
            printf "  DARWIN:%s appears %d times\n", pmid, count[pmid];
            duplicates++;
        }
    }
    exit duplicates;
}
' "$CLUSTERS_TMP" - > /dev/null

if [ $? -gt 0 ]; then
    printf "${RED}ERROR: Duplicate PMIDs found in metrics.c${NC}\n"
    errors=$((errors + $?))
else
    printf "${GREEN}✓ No duplicate PMIDs found${NC}\n"
fi
printf "\n"

# Validation 4: Check for gaps in item sequences (informational)
printf "Check 4: Item sequence gaps (informational)\n"
GAPS_FOUND=0

for cluster_num in $(awk '{print $2}' "$CLUSTERS_TMP" | sort -n | uniq); do
    items=$(grep "^${cluster_num}:" "$METRICS_PMIDS" | \
            sed "s/^${cluster_num}://" | \
            sort -n | \
            tr '\n' ' ')

    if [ -n "$items" ]; then
        max_item=0
        for item in $items; do
            if [ "$item" -gt "$max_item" ]; then
                max_item=$item
            fi
        done

        gaps=""
        i=0
        while [ $i -le "$max_item" ]; do
            found=0
            for item in $items; do
                if [ "$item" -eq "$i" ]; then
                    found=1
                    break
                fi
            done
            if [ $found -eq 0 ]; then
                gaps="$gaps $i"
            fi
            i=$((i + 1))
        done

        if [ -n "$gaps" ]; then
            cluster_name=$(awk -v num="$cluster_num" '$2 == num {print $1; exit}' "$CLUSTERS_TMP")
            gap_count=$(echo "$gaps" | wc -w | tr -d ' ')
            printf "${YELLOW}INFO: Cluster %s (%s) has %d gaps in item sequence 0-%d${NC}\n" \
                   "$cluster_num" "$cluster_name" "$gap_count" "$max_item"
            GAPS_FOUND=1
            warnings=$((warnings + 1))
        fi
    fi
done

if [ $GAPS_FOUND -eq 0 ]; then
    printf "${GREEN}✓ No item sequence gaps${NC}\n"
fi
printf "\n"

# Summary
printf "═══════════════════════════════════════\n"
if [ $errors -eq 0 ]; then
    printf "${GREEN}VALIDATION PASSED${NC}\n"
    printf "All PMID definitions are consistent\n"
    if [ $warnings -gt 0 ]; then
        printf "(%d warnings - review recommended)\n" "$warnings"
    fi
    exit 0
else
    printf "${RED}VALIDATION FAILED${NC}\n"
    printf "Found %d error(s), %d warning(s)\n" "$errors" "$warnings"
    exit 1
fi
