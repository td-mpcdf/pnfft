## Process this file with automake to create Makefile.in

# Makefile for PNFFT.

LIBTOOL_DEPS = @LIBTOOL_DEPS@

libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck

# Let aclocal find the add-on macros.
ACLOCAL_AMFLAGS = -I m4

# Subdirectories
SUBDIRS = kernel util api .

if ENABLE_TESTS
SUBDIRS += tests
endif

EXTRA_DIST = bootstrap.sh CONVENTIONS pnfft.pc.in

# Libraries that are built and installed.
# set library name according to precision
if ENABLE_LIBRARY_INSTALL
lib_LTLIBRARIES = lib@PNFFT_PREFIX@pnfft@PREC_SUFFIX@.la
else
noinst_LTLIBRARIES = lib@PNFFT_PREFIX@pnfft@PREC_SUFFIX@.la
endif

# Libtool convenience libraries.
lib@PNFFT_PREFIX@pnfft@PREC_SUFFIX@_la_SOURCES =
lib@PNFFT_PREFIX@pnfft@PREC_SUFFIX@_la_LIBADD = \
	kernel/libkernel.la \
	util/libutil.la \
	api/libapi.la
#	tests/libtests.la
lib@PNFFT_PREFIX@pnfft@PREC_SUFFIX@_la_LDFLAGS = -no-undefined -version-info @SHARED_VERSION_INFO@

# Get Fortran compile rules that include preprocessing.
include $(top_srcdir)/build-aux/fortran-rules.am

pnfft1@PREC_SUFFIX@.pc: pnfft.pc
	cp -f pnfft.pc pnfft1@PREC_SUFFIX@.pc