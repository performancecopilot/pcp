# Contributing

:tada:Thanks for taking the time to contribute!:tada:

The following is a set of guidelines for contributing to hiredis-cluster.

The basics about setting up the project, building and testing is covered in
the [README](README.md).

## Coding conventions

### Code style

Adhere to the existing coding style and make sure to mimic best possible.

### Code formatting

To have a common look-and-feel [clang-format](https://clang.llvm.org/docs/ClangFormat.html)
is used for code formatting. The formatting rules can be applied to the
source code by running the following make target in your build directory:

```sh
$ make format
```

### Test coverage

Make sure changes are covered by tests.
Code coverage instrumentation can be enabled using a build option and
a detailed html report can be viewed using following example:

```sh
$ mkdir -p build; cd build
$ cmake -DENABLE_COVERAGE=ON ..
$ make all test coverage
$ xdg-open ./coverage.html
```

The report generation requires that [gcovr](https://gcovr.com/en/stable/index.html)
is installed in your path. Any reporting tool of choice can be used, as long as
it reads .gcda and .gcno files created during the test run.

## Submitting changes

* Run the formatter before committing when contributing to this project (`make format`).
* Cover new behaviour with tests when possible.

## Links

* [clang-format](https://apt.llvm.org/) for code formatting
