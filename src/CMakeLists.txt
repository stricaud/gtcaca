include_directories(AFTER ${LIBCACA_INCLUDE_DIRS})
include_directories(AFTER "include/" ${GTCACA_INCLUDE_DIRS})

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
)

set(GTCACA_LINK_LIBRARIES
  ${LIBCACA_LDFLAGS}
)

add_library(gtcaca SHARED ${GTCACA_LIB_SRC})
set_target_properties(gtcaca PROPERTIES SOVERSION 1)
target_link_libraries(gtcaca ${GTCACA_LINK_LIBRARIES})

install(TARGETS gtcaca DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR} COMPONENT Libraries)
install(FILES ${GTCACA_HEADERS} DESTINATION ${CMAKE_INSTALL_PREFIX}/include/gtcaca COMPONENT Headers)
