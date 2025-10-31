# usdt.h: Single-header USDT triggering library

This single-header library is providing a set of macro-based APIs for user
space applications to define and trigger USDTs (User Statically-Defined
Tracepoints, or simply "user space tracepoints"). All the API documentation is
self-contained inside [usdt.h](https://github.com/libbpf/usdt/blob/master/usdt.h)
in the form of C comments, please read through them to understand three
different ways USDTs can be defined and used.

USDT semaphores are an important (though, optional) part of USDTs usage, and
are fully supported in two different modes: implicit semaphores meant for the
best possible usability, and explicit user-defined semaphores, meant for more
advanced use cases that require sharing common USDT semaphore across a few
related USDTs. Refer to documentation for an example scenario.

It is intentional that usdt.h has no dependencies and is single-file. It will
remain single-file library going forward. This is meant to cause minimal
hurdles in adoption inside applications. It is encouraged to just copy the
file into your project and bypass any sort of dependency management or
packaging issues.

usdt.h is versioned. Initial release is designated v0.1.0 version to give it
a bit of time to find any corner case bugs, but other than that it's fully
feature-complete version and is already used in production across variety of
applications. The changelog is embedded into the documentation comments. There
are also `USDT_{MAJOR,MINOR,PATCH}_VERSION` macros defined, if you need to
take that into account at compilation time.

There are a set of tests inside `tests/` subdirectory meant to validate
various aspects of the library implementation and end-to-end API usage. Feel
free to consult it for various API usage questions, but otherwise it's
completely stand-along from `usdt.h` itself and user applications are not
expected to copy tests over into their applications. [usdt.h](https://github.com/libbpf/usdt/blob/master/usdt.h)
is the only file you need to make use of USDTs in your application.

# Why another USDT library?

The goal for this re-implementation is:
  - 100% compatibility with existing USDT tracing tools;
  - more user-friendly API when it comes to USDT support;
  - no dependencies, simple integration: single file, just copy it into your project;
  - allow for sharing USDT semaphore between a group of related USDTs (think request start and end USDTs).

## usdt.h vs [systemtap's sdt.h](https://sourceware.org/git/?p=systemtap.git;a=blob;f=includes/sys/sdt.h):
- ease of deployment and application integration:
    - usdt.h is a stand-alone single header file and encourages copyng that file into user application with no dependency management;
    - systemtap's sdt.h needs extra sdt-config.h header and comes as part of larger `systemtap-sdt-devel` package;
- USDT semaphore usability:
    - systemtap's sdt.h has cumbersome USDT semaphore support: user has to define semaphore variable explicitly and put it into a special ELF section (".probes");
    - usdt.h doesn't require explicit USDT semaphore definition, `USDT_WITH_SEMA()` does that transparently (but there is also `USDT_WITH_EXPLICIT_SEMAPHORE()`, if necessary);
    - for systemtap's sdt.h implementation, it's all or nothing when it comes to USDTs with semaphores: either all USDTs within a .c/.cpp file use USDT semaphores, or none do, and it's controlled by extra `#define _SDT_HAS_SEMAPHORES`;
    - usdt.h allows to mix USDTs with and without semaphore arbitrarily, just use `USDT()` and `USDT_WITH_SEMA()` as necessary;
    - further, usdt.h allows a new use case of sharing common USDT semaphore between multiple related USDTs (e.g., request start and end USDTs, capturing request latency: if either is activated, application needs to know and collect timestamps); use `USDT_DEFINE_SEMA()` and `USDT_WITH_EXPLICIT_SEMA()` APIs for this;
- assembly support:
    - systemtap's sdt.h **does** support USDTs inside assembly code;
    - usdt.h **does not** support usage from assembly code (stick to systemtap's implementation if you need USDTs in assembly code);

## usdt.h vs [folly's StaticTracepoint.h](https://github.com/facebook/folly/blob/main/folly/tracing/StaticTracepoint.h):
- ease of deployment and application integration:
    - usdt.h is a stand-alone single header file and encourages copyng that file into user application with no dependency management;
    - folly's StaticTracepoint.h also needs StaticTracepoint-ELF.h, and comes as part of larger folly library;
- features, language, architecture support:
    - folly's implementation is limited to 9 USDT arguments, while usdt.h and systemtap's sdt.h supports 12 arguments;
    - folly's implementation cuts corners and doesn't record signedness of USDT arguments (all args are recorded as signed by folly's implementation);
    - folly's implementation self-limits itself to just x86/x86-64 and ARM/ARM64 architectures;
    - folly's implementation is C++-specific;
    - usdt.h has none of the above limitations;
- USDT semaphore usability:
    - folly's implementation allows mixing and matching USDTs with and without semaphores, but requires explicit USDT semaphore definition with `FOLLY_SDT_DEFINE_SEMAPHORE()` API;
    - usdt.h supports implicitly defined USDT semaphore with `USDT_WITH_SEMA()` (and matching `USDT_IS_ACTIVE()`);
    - usdt.h also supports explicitly defined USDTs, just like folly's implementation, with `USDT_DEFINE_SEMA()`, `USDT_WITH_EXPLICIT_SEMA()`, and `USDT_SEMA_IS_ACTIVE()` combo of APIs;
    - usdt.h supports sharing explicitly defined USDT semaphore across multiple related USDTS (see above sdt.h comparison);
    - folly's implementation only has 1:1 mapping between USDT and USDT semaphore, sharing USDT semaphore is not supported;
- test coverage:
    - folly's implementation, while is used in large production applications and is battle-tested, doesn't have extensive tests validating various corner cases;
    - usdt.h comes with various tests validating various aspects of API and usage, down to advanced and pretty obscure cases; and that across both C and C++ languages.

## Licensing

This library is released under BSD 2-Clause [license](https://github.com/libbpf/usdt/blob/master/LICENSE).
