enable_testing()

include_directories(AFTER ${LIBCACA_INCLUDE_DIRS})
include_directories(AFTER ${GTCACA_INCLUDE_DIRS})

set(GTCACA_LINK_LIBRARIES
  ${LIBCACA_LDFLAGS}
  ${GTCACA_LIBRARY}
  )

add_executable(tcaca tcaca.c)
target_link_libraries(tcaca ${LIBCACA_LINK_LIBRARIES} ${CMAKE_DL_LIBS})
add_test(TestCaca tcaca)

add_executable(simple simple.c)
target_link_libraries(simple ${GTCACA_LINK_LIBRARIES} ${CMAKE_DL_LIBS})
add_test(Simple simple)

add_executable(simple_layout simple_layout.c)
target_link_libraries(simple_layout ${GTCACA_LINK_LIBRARIES} ${CMAKE_DL_LIBS})
add_test(SimpleLayout simple_layout)

add_executable(textlist textlist.c)
target_link_libraries(textlist ${GTCACA_LINK_LIBRARIES} ${CMAKE_DL_LIBS})
add_test(Textlist textlist)

add_executable(2textlists2windows 2textlists2windows.c)
target_link_libraries(2textlists2windows ${GTCACA_LINK_LIBRARIES} ${CMAKE_DL_LIBS})
add_test(2textlists2windows 2textlists2windows)

add_executable(button button.c)
target_link_libraries(button ${GTCACA_LINK_LIBRARIES} ${CMAKE_DL_LIBS})
add_test(Button button)

add_executable(demo demo.c)
target_link_libraries(demo ${GTCACA_LINK_LIBRARIES} ${CMAKE_DL_LIBS})

add_executable(keytest keytest.c)
target_link_libraries(keytest ${LIBCACA_LINK_LIBRARIES} ${CMAKE_DL_LIBS})

add_executable(image image.c)
target_link_libraries(image ${GTCACA_LINK_LIBRARIES} ${CMAKE_DL_LIBS})

add_executable(widgets widgets.c)
target_link_libraries(widgets ${GTCACA_LINK_LIBRARIES} ${CMAKE_DL_LIBS})

add_executable(demo_login demo_login.c)
target_link_libraries(demo_login ${GTCACA_LINK_LIBRARIES} ${CMAKE_DL_LIBS})

add_executable(demo_settings demo_settings.c)
target_link_libraries(demo_settings ${GTCACA_LINK_LIBRARIES} ${CMAKE_DL_LIBS})

add_executable(demo_monitor demo_monitor.c)
target_link_libraries(demo_monitor ${GTCACA_LINK_LIBRARIES} ${CMAKE_DL_LIBS})

add_executable(demo_json demo_json.c)
target_link_libraries(demo_json ${GTCACA_LINK_LIBRARIES} ${CMAKE_DL_LIBS})

add_executable(demo_charts demo_charts.c)
target_link_libraries(demo_charts ${GTCACA_LINK_LIBRARIES} ${CMAKE_DL_LIBS} m)

add_executable(demo_tree demo_tree.c)
target_link_libraries(demo_tree ${GTCACA_LINK_LIBRARIES} ${CMAKE_DL_LIBS})

add_executable(demo_table demo_table.c)
target_link_libraries(demo_table ${GTCACA_LINK_LIBRARIES} ${CMAKE_DL_LIBS})

add_executable(demo_map demo_map.c)
target_link_libraries(demo_map ${GTCACA_LINK_LIBRARIES} ${CMAKE_DL_LIBS})
