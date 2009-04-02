## for automake 1.9.6
##
## see http://www.mail-archive.com/automake@gnu.org/msg13232.html
docdir = ${datadir}/doc/${PACKAGE}

SUBDIRS =	cmake src lib plugins examples scripts tests m4

EXTRA_DIST = \
		config.h.cmake \
		COPYING.lfs \
		Doxyfile \
		README.TESTS \
		autogen.sh \
		mysql-proxy.spec \
		winbuild.bat

ACLOCAL_AMFLAGS = -I m4

dist-hook:
	rm -rf `find $(distdir) -type d -name .bzr -print`

## this check is in the toplevel Makefile intentionally,
## to catch the case when Lua is statically linked more than once 
## TODO: How do I wrestle the platform's shared library extension from automake's dead hands?
##       And why does libtool generate .so files on OS X?
installcheck:
	if test  `find $(DESTDIR)$(libdir) \( -name "*.so" -or -name "*.dylib" \) -and -type f | xargs nm -A -P | grep "_lua_gc T" | wc -l` -gt 1 ; then \
		echo "ERROR: multiple libraries export Lua library symbols. Do not statically link against Lua more than once!"; \
		echo "The following libraries contain Lua symbols:"; \
		find $(DESTDIR)$(libdir) -name "*.so" -or -name "*.dylib" -and -type f | xargs nm -A -P | grep "_lua_gc T" | cut -f 1 -d: ; \
		exit 1 ; \
	else:; fi
