include_directories(AFTER ${LIBCACA_INCLUDE_DIRS})
include_directories(AFTER "include/" ${GTCACA_INCLUDE_DIRS})

set(GTCACA_LIB_SRC
  main.c
  iniparse.c
  log.c

  application.c
  button.c
  # calendar.c
  # combobox.c
  # dialog.c
  # entry.c
  # filechooserdialog.c
  # image.c
  label.c
  # menu.c
  # progressbar.c
  # radiobutton.c
  # statusbar.c
  textlist.c
  # textview.c
  theme.c
  widget.c
  window.c
)

set(GTCACA_HEADERS
  include/gtcaca/application.h
  include/gtcaca/button.h
  include/gtcaca/iniparse.h
  include/gtcaca/label.h
  include/gtcaca/log.h
  include/gtcaca/main.h
  include/gtcaca/textlist.h
  include/gtcaca/theme.h
  include/gtcaca/utarray.h
  include/gtcaca/utlist.h
  include/gtcaca/widget.h
  include/gtcaca/window.h
)

set(GTCACA_LINK_LIBRARIES
  ${LIBCACA_LDFLAGS}
)

add_library(gtcaca SHARED ${GTCACA_LIB_SRC})
set_target_properties(gtcaca PROPERTIES SOVERSION 1)
target_link_libraries(gtcaca ${GTCACA_LINK_LIBRARIES})

install(TARGETS gtcaca DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR} COMPONENT Libraries)
install(FILES ${GTCACA_HEADERS} DESTINATION ${CMAKE_INSTALL_PREFIX}/include/gtcaca COMPONENT Headers)
