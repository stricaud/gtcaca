#include <stdio.h>

#include <caca.h>

#include <gtcaca/theme.h>
#include <gtcaca/main.h>

int gtcaca_init(int *argc, char ***argv)
{

  gmo.dp = caca_create_display(NULL);
  if (!gmo.dp) {
    fprintf(stderr, "Error, cannot create display!\n");
    return -1;
  }

  gmo.cv = caca_get_canvas(gmo.dp);
  
  return 0;
}

void gtcaca_main(void)
{
  int quit = 0;

    while (!quit) {
    caca_event_t ev;
    
    while(caca_get_event(gmo.dp, CACA_EVENT_ANY, &ev, 0)) {
      if (caca_get_event_type(&ev) & CACA_EVENT_KEY_PRESS) {
	switch(caca_get_event_key_ch(&ev)) {
	case 'q':
	case 'Q':
	case CACA_KEY_ESCAPE:
	  quit = 1;
	  break;
	case CACA_KEY_RETURN:
	  break;
	case CACA_KEY_UP:
	  break;
	case CACA_KEY_DOWN:
	  break;
	}
      }
      
      caca_refresh_display(gmo.dp);
    }
  }
  
  caca_free_display(gmo.dp);

}

