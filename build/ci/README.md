# Cloud CI

## Design
The distributions directory contains a directory for each supported distribution.
There are some similarities and also differences between distributions and distribution versions,
therefore we have the `run_script.sh` script, which the appropriate script for a specified distribution.

For example `run_script.sh _provision.sh ubuntu1804` reads the file `distributions/supported_distributions`
and will look for the `_provision.sh` script in the following directories: `ubuntu1804`, `ubuntu`, `debian`
and `default` (in this order) and executes the first encountered script named `_provision.sh`.

Convention: scripts starting with an underscore are meant to be run on the remote server (e.g. `_build.sh`),
scripts not starting with an underscore (e.g. `start_vmss.sh`) should run on the management host (i.e. CI server).

Furthermore, `run_script.sh` exports some common used environment variables like `IMAGE` etc.
