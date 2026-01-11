This directory contains the macOS(Darwin) PMDA source code, which provides native integration
for the macOS operating system.

## Code Style

* Keep method lengths short, with the "Single Responsibility principal" in mind
* Make the method names descriptive and readable
* Keep the code-style inline with other code in this directory
* Code can be reviewed by the `pcp-code-reviewer` sub-agent

## Testing & QA
* Any changes to this PMDA should have unit and/or integration tests added.
* All unit & integration testing infrastructure is located in $PROJECT_ROOT/scripts/darwin/ directory tree
* The existing overarching full PCP QA system is currently difficult/impossible to run on macOS - and is heavy-weight, so do not use that locally.  Instead, you should use the macOS-specific Tart VM/CirrusLabs setup - the `macos-darwin-pmda-qa` sub-agent will do this for you this and receive test results back that you can analyse


