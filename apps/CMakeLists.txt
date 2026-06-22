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
  cacamacs/edit.c cacamacs/search.c cacamacs/keymap.c cacamacs/lang.c cacamacs/ui.c)
target_include_directories(cacamacs PRIVATE cacamacs)
target_link_libraries(cacamacs ${GTCACA_LINK_LIBRARIES} ${CMAKE_DL_LIBS})

add_executable(cacasheet cacasheet.c)
target_link_libraries(cacasheet ${GTCACA_LINK_LIBRARIES} ${CMAKE_DL_LIBS})
