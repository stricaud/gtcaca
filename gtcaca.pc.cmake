Name: gtcaca
Description: Library to create text-based interfaces using widgets
Version: ${GTCACA_VERSION}
Requires: caca >= 0.99
Conflicts:
Libs: -L${CMAKE_INSTALL_FULL_LIBDIR} -lgtcaca
Libs.private: @ZLIB_LIBS@
Cflags: -I${CMAKE_INSTALL_PREFIX}/include
