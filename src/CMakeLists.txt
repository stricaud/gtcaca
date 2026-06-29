include_directories(AFTER ${LIBCACA_INCLUDE_DIRS})
include_directories(AFTER "include/" ${GTCACA_INCLUDE_DIRS})
if(ONIG_FOUND)
  include_directories(AFTER ${ONIG_INCLUDE_DIRS})
endif()

set(GTCACA_LIB_SRC
  main.c
  iniparse.c
  log.c

  application.c
  button.c
  checkbox.c
  combobox.c
  entry.c
  label.c
  menu.c
  progressbar.c
  radiobutton.c
  statusbar.c
  textlist.c
  textview.c
  theme.c
  widget.c
  window.c
  image.c
  spinner.c
  scale.c
  spinbutton.c
  switch.c
  frame.c
  separator.c
  expander.c
  box.c
  editor.c
  editor_lang.c
  editor_autoc.c
  editor_view.c
  editor_json.c
  editor_tm.c
  editor_search.c
  editor_ops.c
  json.c
  sparkline.c
  gauge.c
  barchart.c
  tree.c
  table.c
  map.c
  tabs.c
  mindmap.c
  segdisplay.c
  linechart.c
  hexview.c
  calendar.c
  dialog.c
  filechooser.c
)

set(GTCACA_HEADERS
  include/gtcaca/application.h
  include/gtcaca/button.h
  include/gtcaca/checkbox.h
  include/gtcaca/combobox.h
  include/gtcaca/entry.h
  include/gtcaca/iniparse.h
  include/gtcaca/label.h
  include/gtcaca/log.h
  include/gtcaca/main.h
  include/gtcaca/menu.h
  include/gtcaca/progressbar.h
  include/gtcaca/radiobutton.h
  include/gtcaca/statusbar.h
  include/gtcaca/textlist.h
  include/gtcaca/textview.h
  include/gtcaca/theme.h
  include/gtcaca/utarray.h
  include/gtcaca/utlist.h
  include/gtcaca/widget.h
  include/gtcaca/window.h
  include/gtcaca/image.h
  include/gtcaca/spinner.h
  include/gtcaca/scale.h
  include/gtcaca/spinbutton.h
  include/gtcaca/switch.h
  include/gtcaca/frame.h
  include/gtcaca/separator.h
  include/gtcaca/expander.h
  include/gtcaca/box.h
  include/gtcaca/editor.h
  include/gtcaca/json.h
  include/gtcaca/sparkline.h
  include/gtcaca/gauge.h
  include/gtcaca/barchart.h
  include/gtcaca/tree.h
  include/gtcaca/table.h
  include/gtcaca/map.h
  include/gtcaca/tabs.h
  include/gtcaca/mindmap.h
  include/gtcaca/segdisplay.h
  include/gtcaca/linechart.h
  include/gtcaca/hexview.h
  include/gtcaca/calendar.h
  include/gtcaca/dialog.h
  include/gtcaca/filechooser.h
)

set(GTCACA_LINK_LIBRARIES
  ${LIBCACA_LDFLAGS}
  ${ONIG_LDFLAGS}
)

add_library(gtcaca SHARED ${GTCACA_LIB_SRC})
set_target_properties(gtcaca PROPERTIES SOVERSION 1)
target_link_libraries(gtcaca ${GTCACA_LINK_LIBRARIES})

install(TARGETS gtcaca DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Libraries)
install(FILES ${GTCACA_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/gtcaca COMPONENT Headers)
