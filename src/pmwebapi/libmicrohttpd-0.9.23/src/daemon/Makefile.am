if USE_PRIVATE_PLIBC_H
 PLIBC_INCLUDE = -I$(top_srcdir)/src/include/plibc
endif

AM_CPPFLAGS = \
  $(PLIBC_INCLUDE) \
  -I$(top_srcdir)/src/include \
  -I$(top_srcdir)/src/daemon \
  @LIBGCRYPT_CFLAGS@ 

EXTRA_DIST = EXPORT.sym

lib_LTLIBRARIES = \
  libmicrohttpd.la


libmicrohttpd_la_SOURCES = \
  connection.c connection.h \
  reason_phrase.c reason_phrase.h \
  daemon.c  \
  internal.c internal.h \
  memorypool.c memorypool.h \
  response.c response.h
libmicrohttpd_la_LDFLAGS = \
  $(MHD_LIB_LDFLAGS) \
  -version-info @LIB_VERSION_CURRENT@:@LIB_VERSION_REVISION@:@LIB_VERSION_AGE@

if USE_COVERAGE
  AM_CFLAGS = --coverage
endif

if !HAVE_TSEARCH
libmicrohttpd_la_SOURCES += \
  tsearch.c tsearch.h
endif

if HAVE_POSTPROCESSOR
libmicrohttpd_la_SOURCES += \
  postprocessor.c
endif

if ENABLE_DAUTH
libmicrohttpd_la_SOURCES += \
  digestauth.c \
  md5.c md5.h
endif

if ENABLE_BAUTH
libmicrohttpd_la_SOURCES += \
  basicauth.c \
  base64.c base64.h
endif

if ENABLE_HTTPS
libmicrohttpd_la_SOURCES += \
  connection_https.c connection_https.h
libmicrohttpd_la_LIBADD = -lgnutls @LIBGCRYPT_LIBS@
endif



check_PROGRAMS = \
  daemon_test 

if HAVE_POSTPROCESSOR
check_PROGRAMS += \
  postprocessor_test \
  postprocessor_large_test 
endif

TESTS = $(check_PROGRAMS)

daemon_test_SOURCES = \
  daemon_test.c
daemon_test_LDADD = \
  $(top_builddir)/src/daemon/libmicrohttpd.la 

postprocessor_test_SOURCES = \
  postprocessor_test.c
postprocessor_test_LDADD = \
  $(top_builddir)/src/daemon/libmicrohttpd.la 

postprocessor_large_test_SOURCES = \
  postprocessor_large_test.c
postprocessor_large_test_LDADD = \
  $(top_builddir)/src/daemon/libmicrohttpd.la 
