enable_testing()

include_directories(AFTER ${LIBCACA_INCLUDE_DIRS})
include_directories(AFTER ${GTCACA_INCLUDE_DIRS})

set(GTCACA_LINK_LIBRARIES
  ${LIBCACA_LDFLAGS}
  ${GTCACA_LIBRARY}
  )

add_executable(simple simple.c)
target_link_libraries(simple ${GTCACA_LINK_LIBRARIES} ${CMAKE_DL_LIBS})
add_test(Simple simple)

add_executable(textlist textlist.c)
target_link_libraries(textlist ${GTCACA_LINK_LIBRARIES} ${CMAKE_DL_LIBS})
add_test(Textlist textlist)

add_executable(2textlists2windows 2textlists2windows.c)
target_link_libraries(2textlists2windows ${GTCACA_LINK_LIBRARIES} ${CMAKE_DL_LIBS})
add_test(2textlists2windows 2textlists2windows)

add_executable(button button.c)
target_link_libraries(button ${GTCACA_LINK_LIBRARIES} ${CMAKE_DL_LIBS})
add_test(Button button)
