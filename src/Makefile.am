AM_CFLAGS = $(VARNISHAPI_CFLAGS) $(CODE_COVERAGE_CFLAGS) -Wall -DEV_COMPAT3=1
AM_LDFLAGS = $(VARNISHAPI_LIBS) $(VMOD_LDFLAGS) $(CODE_COVERAGE_LDFLAGS)

vmod_LTLIBRARIES = libvmod_valkey.la

libvmod_valkey_la_SOURCES = \
	crc16.c crc16.h \
	sha1.c sha1.h \
	cluster.c cluster.h \
	core.c core.h \
	sentinel.c sentinel.h \
	vmod_valkey.c

nodist_libvmod_valkey_la_SOURCES = \
	vcc_valkey_if.c \
	vcc_valkey_if.h

dist_man_MANS = vmod_valkey.3

@BUILD_VMOD_VALKEY@

AM_TESTS_ENVIRONMENT = \
	PATH="$(VMOD_TEST_PATH)" \
	LD_LIBRARY_PATH="$(VARNISH_LIBRARY_PATH)"
TEST_EXTENSIONS = .vtc
VTC_LOG_COMPILER = $(abs_srcdir)/tests/runner.sh varnishtest
AM_VTC_LOG_FLAGS = -v -Dvmod_valkey="$(VMOD_VALKEY)"

TESTS = @VMOD_TESTS@

EXTRA_DIST = \
	tests/assets/hashslot-keys.txt \
	tests/assets/tls-ca-certificate.crt \
	tests/assets/tls-ca-certificate.key \
	tests/assets/tls-certificate.crt \
	tests/assets/tls-certificate.key \
	tests/runner.sh \
	vmod_valkey.vcc \
	$(VMOD_TESTS)

DISTCLEANFILES = vmod_vcs_version.txt
