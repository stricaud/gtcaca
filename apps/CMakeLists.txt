include_directories(AFTER ${LIBCACA_INCLUDE_DIRS})
include_directories(AFTER ${GTCACA_INCLUDE_DIRS})

set(GTCACA_LINK_LIBRARIES
  ${LIBCACA_LDFLAGS}
  ${GTCACA_LIBRARY}
)

add_executable(devhelp
  devhelp.c
  ${CMAKE_SOURCE_DIR}/docs/gtcaca_api.c
)

target_include_directories(devhelp PRIVATE ${CMAKE_SOURCE_DIR}/docs)

target_link_libraries(devhelp ${GTCACA_LINK_LIBRARIES} ${CMAKE_DL_LIBS})

add_executable(cacamacs
  cacamacs/cacamacs.c cacamacs/games.c
  cacamacs/edit.c cacamacs/search.c cacamacs/keymap.c cacamacs/lang.c cacamacs/ui.c
  cacamacs/spell.c cacamacs/theme.c)
target_include_directories(cacamacs PRIVATE cacamacs)
target_link_libraries(cacamacs ${GTCACA_LINK_LIBRARIES} ${CMAKE_DL_LIBS})
# the editor's binary is `ccm` (shorter to type); the CMake target stays cacamacs
set_target_properties(cacamacs PROPERTIES OUTPUT_NAME ccm)

add_executable(cacasheet cacasheet.c)
target_link_libraries(cacasheet ${GTCACA_LINK_LIBRARIES} ${CMAKE_DL_LIBS})

add_executable(rfmind rfmind.c)
target_link_libraries(rfmind ${GTCACA_LINK_LIBRARIES} ${CMAKE_DL_LIBS})

add_executable(rtodo rtodo.c)
target_link_libraries(rtodo ${GTCACA_LINK_LIBRARIES} ${CMAKE_DL_LIBS})

# Install the terminal apps so they land on PATH (e.g. /usr/local/bin) and ship
# in the package. The rpath lets them find libgtcaca.dylib in the sibling libdir.
if(APPLE)
  set_target_properties(cacamacs cacasheet rfmind rtodo PROPERTIES
    INSTALL_RPATH "@loader_path/../${CMAKE_INSTALL_LIBDIR}")
endif()
install(TARGETS cacamacs cacasheet rfmind rtodo
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT Applications)
