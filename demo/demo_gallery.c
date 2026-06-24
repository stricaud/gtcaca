/* demo_gallery — flip through every gtcaca widget one at a time.
   Left/Right (or p/n) switch widget, q quits. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <caca.h>
#include <gtcaca/main.h>
#include <gtcaca/label.h>
#include <gtcaca/button.h>
#include <gtcaca/entry.h>
#include <gtcaca/checkbox.h>
#include <gtcaca/radiobutton.h>
#include <gtcaca/combobox.h>
#include <gtcaca/textlist.h>
#include <gtcaca/textview.h>
#include <gtcaca/progressbar.h>
#include <gtcaca/scale.h>
#include <gtcaca/spinbutton.h>
#include <gtcaca/switch.h>
#include <gtcaca/frame.h>
#include <gtcaca/separator.h>
#include <gtcaca/expander.h>
#include <gtcaca/editor.h>
#include <gtcaca/sparkline.h>
#include <gtcaca/gauge.h>
#include <gtcaca/barchart.h>
#include <gtcaca/tree.h>
#include <gtcaca/table.h>
#include <gtcaca/map.h>
#include <gtcaca/tabs.h>
#include <gtcaca/calendar.h>
#include <gtcaca/mindmap.h>
#include <gtcaca/segdisplay.h>
#include <gtcaca/linechart.h>
#include <gtcaca/dialog.h>
#include <gtcaca/filechooser.h>

/* ── models for tree / table ── */
static int ndepth(void*n){return n?(int)((uintptr_t)n>>56):0;}
static unsigned long nord(void*n){return n?((uintptr_t)n&0x00FFFFFFFFFFFFFFULL):0UL;}
static void*mknode(int d,unsigned long o){return (void*)(((uintptr_t)d<<56)|o);}
static long tc_count(gtcaca_tree_model_t*m,void*n){(void)m;return ndepth(n)<3?4:0;}
static void*tc_child(gtcaca_tree_model_t*m,void*n,long i){(void)m;return mknode(ndepth(n)+1,nord(n)*4+(unsigned long)i);}
static int tc_has(gtcaca_tree_model_t*m,void*n){(void)m;return ndepth(n)<3;}
static void tc_label(gtcaca_tree_model_t*m,void*n,char*b,int l){(void)m;int d=ndepth(n);
  const char*k=d==1?"Folder":d==2?"Subfolder":"File";snprintf(b,l,"%s %lu",k,nord(n));}
static gtcaca_tree_model_t TREE={tc_count,tc_child,tc_label,tc_has,NULL};
static long tb_rows(gtcaca_table_model_t*m){(void)m;return 5;}
static int tb_cols(gtcaca_table_model_t*m){(void)m;return 3;}
static void tb_hdr(gtcaca_table_model_t*m,int c,char*b,int n){(void)m;const char*h[]={"Name","Role","Score"};snprintf(b,n,"%s",h[c]);}
static void tb_cell(gtcaca_table_model_t*m,long r,int c,char*b,int n){(void)m;
  static const char*nm[]={"Ada","Linus","Grace","Ken","Margaret"},*ro[]={"Eng","Kernel","Navy","Unix","Apollo"};
  static const int sc[]={98,95,99,91,97};
  if(c==0)snprintf(b,n,"%s",nm[r]);else if(c==1)snprintf(b,n,"%s",ro[r]);else snprintf(b,n,"%d",sc[r]);}
static gtcaca_table_model_t TBL={tb_rows,tb_cols,tb_hdr,tb_cell,NULL};

/* ── per-widget render functions: build at (x,y) with size (w,h) and draw ── */
static void r_label(int x,int y,int w,int h){(void)w;(void)h;gtcaca_label_draw(gtcaca_label_new(NULL,"Hello, gtcaca!",x,y));}
static void r_button(int x,int y,int w,int h){(void)w;(void)h;gtcaca_button_widget_t*b=gtcaca_button_new(NULL,"OK",x,y);b->has_focus=1;gtcaca_button_draw(b);}
static void r_entry(int x,int y,int w,int h){(void)h;gtcaca_entry_widget_t*e=gtcaca_entry_new(NULL,x,y,w);e->has_focus=1;gtcaca_entry_set_text(e,"user@host");gtcaca_entry_draw(e);}
static void r_check(int x,int y,int w,int h){(void)w;(void)h;gtcaca_checkbox_widget_t*c=gtcaca_checkbox_new(NULL,"Enable sound",x,y);gtcaca_checkbox_set_checked(c,1);gtcaca_checkbox_draw(c);gtcaca_checkbox_draw(gtcaca_checkbox_new(NULL,"Fullscreen",x,y+1));}
static void r_radio(int x,int y,int w,int h){(void)w;(void)h;gtcaca_radiobutton_widget_t*r=gtcaca_radiobutton_new(NULL,"Small",1,x,y);gtcaca_radiobutton_set_active(r);gtcaca_radiobutton_draw(r);gtcaca_radiobutton_draw(gtcaca_radiobutton_new(NULL,"Large",1,x,y+1));}
static void r_combo(int x,int y,int w,int h){(void)h;gtcaca_combobox_widget_t*c=gtcaca_combobox_new(NULL,x,y,w);gtcaca_combobox_append(c,"C");gtcaca_combobox_append(c,"Rust");gtcaca_combobox_append(c,"Go");gtcaca_combobox_draw(c);}
static void r_switch(int x,int y,int w,int h){(void)w;(void)h;gtcaca_switch_widget_t*s=gtcaca_switch_new(NULL,x,y);gtcaca_switch_set_active(s,1);gtcaca_switch_draw(s);}
static void r_scale(int x,int y,int w,int h){(void)h;gtcaca_scale_widget_t*s=gtcaca_scale_new(NULL,x,y,w,0,100,1);s->has_focus=1;gtcaca_scale_set_value(s,60);gtcaca_scale_draw(s);}
static void r_spin(int x,int y,int w,int h){(void)w;(void)h;gtcaca_spinbutton_widget_t*s=gtcaca_spinbutton_new(NULL,x,y,0,100,1);s->has_focus=1;gtcaca_spinbutton_set_value(s,42);gtcaca_spinbutton_draw(s);}
static void r_progress(int x,int y,int w,int h){(void)h;gtcaca_progressbar_widget_t*p=gtcaca_progressbar_new(NULL,x,y,w);gtcaca_progressbar_set_value(p,0.62f);gtcaca_progressbar_draw(p);}
static void r_textlist(int x,int y,int w,int h){(void)w;(void)h;gtcaca_textlist_widget_t*t=gtcaca_textlist_new(NULL,x,y);t->has_focus=1;gtcaca_textlist_append(t,"apple");gtcaca_textlist_append(t,"banana");gtcaca_textlist_append(t,"cherry");gtcaca_textlist_append(t,"date");gtcaca_textlist_draw(t);}
static void r_textview(int x,int y,int w,int h){gtcaca_textview_widget_t*t=gtcaca_textview_new(NULL,x,y,w,h);gtcaca_textview_append(t,"A scrollable, read-only");gtcaca_textview_append(t,"text view. Lines scroll");gtcaca_textview_append(t,"as needed.");gtcaca_textview_draw(t);}
static void r_frame(int x,int y,int w,int h){gtcaca_frame_draw(gtcaca_frame_new(NULL,"Account",x,y,w,h));gtcaca_label_draw(gtcaca_label_new(NULL,"name: ada",x+2,y+2));}
static void r_sep(int x,int y,int w,int h){(void)h;gtcaca_separator_draw(gtcaca_separator_new(NULL,x,y,w,0));}
static void r_expander(int x,int y,int w,int h){(void)h;gtcaca_expander_widget_t*e=gtcaca_expander_new(NULL,"Advanced options",x,y,w);gtcaca_expander_set_expanded(e,1);gtcaca_expander_draw(e);}
static void r_tabs(int x,int y,int w,int h){gtcaca_tabs_widget_t*t=gtcaca_tabs_new(NULL,x,y,w,h);t->has_focus=1;gtcaca_tabs_set_title(t,"view");const char*ti[]={"Files","Edit","Build","Help"};gtcaca_tabs_set_titles(t,ti,4);gtcaca_tabs_set_selected(t,1);gtcaca_tabs_draw(t);}
static void r_spark(int x,int y,int w,int h){gtcaca_sparkline_widget_t*s=gtcaca_sparkline_new(NULL,x,y,w,h);float v[28];int i;for(i=0;i<28;i++)v[i]=(float)(10+9*((i*7)%13));gtcaca_sparkline_set_data(s,v,28);gtcaca_sparkline_draw(s);}
static void r_gauge(int x,int y,int w,int h){(void)h;gtcaca_gauge_widget_t*g=gtcaca_gauge_new(NULL,x,y,w);gtcaca_gauge_set_percent(g,72);gtcaca_gauge_draw(g);}
static void r_bar(int x,int y,int w,int h){gtcaca_barchart_widget_t*b=gtcaca_barchart_new(NULL,x,y,w,h);float v[5]={30,75,45,90,60};const char*l[5]={"Jan","Feb","Mar","Apr","May"};gtcaca_barchart_set_data(b,v,l,5);gtcaca_barchart_draw(b);}
static void r_line(int x,int y,int w,int h){gtcaca_linechart_widget_t*c=gtcaca_linechart_new(NULL,x,y,w,h);gtcaca_linechart_set_title(c,"sin / cos");double a[40],b[40];int i;for(i=0;i<40;i++){a[i]=sin(i*0.3);b[i]=0.6*cos(i*0.3);}gtcaca_linechart_add_series(c,a,40,CACA_LIGHTGREEN);gtcaca_linechart_add_series(c,b,40,CACA_LIGHTRED);gtcaca_linechart_draw(c);}
static void r_seg(int x,int y,int w,int h){gtcaca_segdisplay_widget_t*s=gtcaca_segdisplay_new(NULL,x,y,w,h);gtcaca_segdisplay_set_title(s,"clock");gtcaca_segdisplay_set_text(s,"12:45");gtcaca_segdisplay_draw(s);}
static void r_tree(int x,int y,int w,int h){gtcaca_tree_widget_t*t=gtcaca_tree_new(NULL,x,y,w,h);t->has_focus=1;gtcaca_tree_set_title(t,"files");gtcaca_tree_set_model(t,&TREE);gtcaca_tree_key(t,CACA_KEY_RIGHT,NULL);gtcaca_tree_key(t,CACA_KEY_DOWN,NULL);gtcaca_tree_key(t,CACA_KEY_RIGHT,NULL);gtcaca_tree_draw(t);}
static void r_table(int x,int y,int w,int h){gtcaca_table_widget_t*t=gtcaca_table_new(NULL,x,y,w,h);t->has_focus=1;gtcaca_table_set_title(t,"people");gtcaca_table_set_model(t,&TBL);gtcaca_table_set_current(t,2,1);gtcaca_table_draw(t);}
static void r_map(int x,int y,int w,int h){gtcaca_map_widget_t*m=gtcaca_map_new(NULL,x,y,w,h);m->has_focus=1;gtcaca_map_set_title(m,"world");gtcaca_map_add_world(m,CACA_GREEN);const char*c[]={"London","New York","Tokyo","Sao Paulo","Sydney","Cairo"};int i;for(i=0;i<6;i++)gtcaca_map_add_city(m,c[i],'o',CACA_LIGHTGREEN);gtcaca_map_draw(m);}
static void r_cal(int x,int y,int w,int h){gtcaca_calendar_widget_t*c=gtcaca_calendar_new(NULL,x,y,w,h);c->has_focus=1;gtcaca_calendar_set_date(c,2026,6,25);c->today_y=2026;c->today_m=6;c->today_d=25;gtcaca_calendar_draw(c);}
static void r_mind(int x,int y,int w,int h){gtcaca_mindmap_widget_t*m=gtcaca_mindmap_new(NULL,x,y,w,h);m->has_focus=1;gtcaca_mindmap_set_title(m,"plan");gtcaca_mindmap_set_text(m->root,"Project");
  gtcaca_mm_node_t*a=gtcaca_mindmap_add_child(m,m->root,"Design");gtcaca_mindmap_add_child(m,a,"UI");gtcaca_mindmap_add_child(m,a,"API");
  gtcaca_mm_node_t*b=gtcaca_mindmap_add_child(m,m->root,"Build");gtcaca_mindmap_add_child(m,b,"CI");gtcaca_mindmap_add_child(m,m->root,"Ship");gtcaca_mindmap_select(m,a);gtcaca_mindmap_draw(m);}
static void r_editor(int x,int y,int w,int h){gtcaca_editor_widget_t*e=gtcaca_editor_new(NULL,x,y,w,h);e->has_focus=1;gtcaca_editor_set_text(e,"int main(void) {\n    printf(\"hi\\n\");\n    return 0;\n}\n");gtcaca_editor_draw(e);}
static void r_dialog(int x,int y,int w,int h){gtcaca_dialog_widget_t*d=gtcaca_dialog_new(NULL,x,y,w,h);const char*bb[]={"OK","Cancel"};gtcaca_dialog_set(d,"Confirm","Save changes before quitting?",bb,2);d->sel=0;gtcaca_dialog_draw(d);}
static void r_fc(int x,int y,int w,int h){gtcaca_filechooser_widget_t*f=gtcaca_filechooser_new(NULL,x,y,w,h);f->has_focus=1;gtcaca_filechooser_set_dir(f,".");gtcaca_filechooser_key(f,CACA_KEY_DOWN,NULL);gtcaca_filechooser_draw(f);}

typedef struct { const char *name, *desc; int w, h; void (*render)(int,int,int,int); } page_t;
static page_t PAGES[] = {
  {"Label","static text",26,1,r_label},
  {"Button","push button (focused)",10,3,r_button},
  {"Entry","single-line text input",28,1,r_entry},
  {"Checkbox","toggle with a label",24,2,r_check},
  {"Radio button","exclusive choice in a group",20,2,r_radio},
  {"Combo box","drop-down choices",22,1,r_combo},
  {"Switch","on/off toggle",14,1,r_switch},
  {"Scale","slider over a range",34,1,r_scale},
  {"Spin button","numeric +/- field",16,1,r_spin},
  {"Progress bar","determinate progress",34,1,r_progress},
  {"Text list","selectable, scrollable list",22,4,r_textlist},
  {"Text view","scrollable read-only text",34,5,r_textview},
  {"Frame","titled grouping box",30,6,r_frame},
  {"Separator","divider rule",30,1,r_sep},
  {"Expander","collapsible section",30,1,r_expander},
  {"Tabs","tabbed bar",44,3,r_tabs},
  {"Sparkline","compact inline trend",36,6,r_spark},
  {"Gauge","percentage bar",34,1,r_gauge},
  {"Bar chart","labelled vertical bars",36,9,r_bar},
  {"Line chart","auto-scaled XY plot",52,12,r_line},
  {"Seven-segment","big LED digits",30,9,r_seg},
  {"Tree","lazy model/view hierarchy",36,10,r_tree},
  {"Table","lazy model/view grid",36,9,r_table},
  {"Map","world geoview + gazetteer",62,17,r_map},
  {"Calendar","month grid",30,11,r_cal},
  {"Mind map","FreeMind-style tree",52,9,r_mind},
  {"Editor","full text editor widget",42,7,r_editor},
  {"Dialog","modal message / confirm",36,6,r_dialog},
  {"File chooser","modal file picker",48,14,r_fc},
};
#define NPAGES ((int)(sizeof PAGES / sizeof PAGES[0]))

int main(int argc, char **argv)
{
  int page = 0, W, H;
  caca_event_t ev;

  gtcaca_init(&argc, &argv);
  gtcaca_application_new("gtcaca gallery");
  gmo.theme.textview.fg = CACA_LIGHTGRAY; gmo.theme.textview.bg = CACA_BLACK;
  W = caca_get_canvas_width(gmo.cv); H = caca_get_canvas_height(gmo.cv);

  for (;;) {
    page_t *p = &PAGES[page];
    int cw = p->w > W - 2 ? W - 2 : p->w, ch = p->h > H - 5 ? H - 5 : p->h;
    int ox = (W - cw) / 2, oy = 3 + (H - 5 - ch) / 2;
    int x;
    gmo.widgets_list = NULL;                       /* fresh list each page */

    caca_set_color_ansi(gmo.cv, CACA_LIGHTGRAY, CACA_BLACK); caca_clear_canvas(gmo.cv);
    /* header */
    caca_set_color_ansi(gmo.cv, CACA_BLACK, CACA_CYAN);
    for (x = 0; x < W; x++) caca_put_char(gmo.cv, x, 0, ' ');
    caca_printf(gmo.cv, 1, 0, " gtcaca gallery   %d/%d", page + 1, NPAGES);
    caca_set_color_ansi(gmo.cv, CACA_WHITE, CACA_BLACK); caca_set_attr(gmo.cv, CACA_BOLD);
    caca_printf(gmo.cv, 2, 1, "%s", p->name);
    caca_set_attr(gmo.cv, 0); caca_set_color_ansi(gmo.cv, CACA_LIGHTGRAY, CACA_BLACK);
    caca_printf(gmo.cv, 2 + (int)strlen(p->name) + 2, 1, "— %s", p->desc);
    /* the widget */
    p->render(ox, oy, cw, ch);
    /* footer */
    caca_set_color_ansi(gmo.cv, CACA_BLACK, CACA_CYAN);
    for (x = 0; x < W; x++) caca_put_char(gmo.cv, x, H - 1, ' ');
    caca_printf(gmo.cv, 1, H - 1, " <- / -> or p / n : previous / next widget      q : quit ");
    caca_refresh_display(gmo.dp);

    if (!caca_get_event(gmo.dp, CACA_EVENT_KEY_PRESS, &ev, -1)) continue;
    {
      int k = caca_get_event_key_ch(&ev);
      if (k == 'q' || k == 'Q' || k == CACA_KEY_ESCAPE) break;
      else if (k == CACA_KEY_RIGHT || k == 'n' || k == ' ') page = (page + 1) % NPAGES;
      else if (k == CACA_KEY_LEFT  || k == 'p') page = (page - 1 + NPAGES) % NPAGES;
    }
  }
  caca_free_display(gmo.dp);
  return 0;
}
