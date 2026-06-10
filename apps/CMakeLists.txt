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
