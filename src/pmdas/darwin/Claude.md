This directory contains the macOS(Darwin) PMDA source code, which provides native integration
for the macOS operating system.

## CRITICAL: macOS Development Constraints

### PCP Is NOT Installed Locally
**NEVER assume PCP tools (`pminfo`, `pmval`, `pmprobe`) are available on the development host.**

To check available metrics or PMNS structure:
- Read `src/pmdas/darwin/pmns` directly - this is the source of truth
- Do NOT try to run PCP commands locally

### Testing Environments

| Test Type | Where to Run | PCP Required? |
|-----------|--------------|---------------|
| **Unit tests** | Local (`./run-unit-tests.sh`) | No |
| **Integration tests** | Tart VM only (`/macos-qa-test`) | Yes (VM has it) |

### Git Commit Requirement

**ALL source code changes MUST be committed to git BEFORE running integration tests.**

The Tart VM clones the git repository - uncommitted local changes are invisible to it.

```bash
# Correct workflow:
git add <changed-files>
git commit -m "description"
/macos-qa-test              # Now VM can see your changes
```

## Code Style

* Keep method lengths short, with the "Single Responsibility principal" in mind
* Make the method names descriptive and readable
* Keep the code-style inline with other code in this directory
* Code can be reviewed by the `pcp-code-reviewer` sub-agent

## Testing & QA

### Unit Tests (Run Locally)
Unit tests do NOT require PCP installation:
```bash
cd src/pmdas/darwin/test && ./run-unit-tests.sh
```

### Integration Tests (Tart VM Only)
Integration tests MUST run in the isolated Tart VM environment. Use the `macos-darwin-pmda-qa` agent or invoke `/macos-qa-test`:

**Before running:**
1. Commit all source changes to git
2. Then run: `/macos-qa-test`

The agent handles `cirrus run --dirty` and reports results.

### Code Review
Use the `pcp-code-reviewer` agent to review changes against PCP project standards


