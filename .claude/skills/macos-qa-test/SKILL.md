---
name: macos-qa-test
description: Run darwin PMDA QA tests in isolated Cirrus VM environment
aliases: [darwin-qa, test-darwin]
---

# Darwin PMDA QA Test Runner

This skill runs the full darwin PMDA build and test suite in an isolated Cirrus CI VM environment.

## Usage

Run this command to execute the darwin PMDA QA tests:

```
/macos-qa-test
```

## What This Does

1. Invokes the `macos-darwin-pmda-qa` agent
2. Runs `cirrus run --dirty` to execute:
   - Full PCP build from source
   - Darwin PMDA compilation
   - Unit tests (via dbpmda)
   - Integration tests (via pminfo/pmval)
3. Reports test results back to you

## Expected Runtime

- Normal execution: 5-6 minutes
- The VM will be provisioned, tests run, and results reported

## Success Indicators

- ✅ All build and test steps pass
- Note: "unmounting the working directory" failure at end is expected/normal

## When to Use

- After making AND COMMITTING changes to `src/pmdas/darwin` or `src/pmdas/darwin_proc`
- To validate new metrics or bug fixes in a clean environment
- Before pushing changes to remote

## CRITICAL: Git Commit Required

**You MUST commit all source code changes to git BEFORE running this skill.**

The Tart VM clones the git repository. Uncommitted local changes are INVISIBLE to the VM.

```bash
# Correct workflow:
git add <changed-files>
git commit -m "Add new metric for X"
/macos-qa-test              # NOW the VM will see your changes
```

If tests fail on code you just wrote, verify you committed the changes first

---

When invoked, immediately use the Task tool to launch the macos-darwin-pmda-qa agent with a simple prompt like "Run darwin PMDA QA tests for recent changes".
