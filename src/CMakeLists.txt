include_directories(AFTER "include/" ${GTCACA_INCLUDE_DIRS})
include_directories(AFTER ${LIBCACA_INCLUDE_DIRS})

set(GTCACA_LIB_SRC
  main.c
  iniparse.c
  
  # button.c
  # calendar.c
  # combobox.c
  # dialog.c
  # entry.c
  # filechooserdialog.c
  # image.c
  # label.c
  # menu.c
  # progressbar.c
  # radiobutton.c
  # statusbar.c
  # textview.c
  theme.c
  # widget.c
  window.c
)

set(GTCACA_LINK_LIBRARIES
  ${LIBCACA_LIBRARIES}
)

add_library(gtcaca SHARED ${GTCACA_LIB_SRC})
set_target_properties(gtcaca PROPERTIES SOVERSION 1)
target_link_libraries(gtcaca ${GTCACA_LINK_LIBRARIES})
