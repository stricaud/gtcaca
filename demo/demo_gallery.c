/* demo_gallery — browse AND test every gtcaca widget.
 *
 *   left pane : a list of widgets (Up/Down to pick)
 *   right pane: the selected widget, live
 *   Tab       : move focus between the list and the widget (so you can use it)
 *   q         : quit  (from the list)
 *
 * When the widget has focus, every key goes to it — type in the entry, expand
 * the tree, flip the calendar, switch tabs, edit the buffer, … Press Tab (or
 * Esc) to return to the list. */
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
#include <gtcaca/menu.h>
#include <strings.h>

struct whdr { gtcaca_widget_type_t type; unsigned int id; int has_focus; };
#define HFOCUS(o,f) (((struct whdr *)(o))->has_focus = (f))
#define WTYPE(o)    (((struct whdr *)(o))->type)

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

/* ── make functions: build at (x,y,w,h), return the (interactive) widget ── */
static void*mk_label(int x,int y,int w,int h){(void)w;(void)h;return gtcaca_label_new(NULL,"Hello, gtcaca!",x,y);}
static void*mk_button(int x,int y,int w,int h){(void)w;(void)h;return gtcaca_button_new(NULL,"OK",x,y);}
static void*mk_entry(int x,int y,int w,int h){(void)h;gtcaca_entry_widget_t*e=gtcaca_entry_new(NULL,x,y,w);gtcaca_entry_set_text(e,"type here");return e;}
static void*mk_check(int x,int y,int w,int h){(void)w;(void)h;gtcaca_checkbox_widget_t*c=gtcaca_checkbox_new(NULL,"Enable sound (Space)",x,y);gtcaca_checkbox_set_checked(c,1);return c;}
static void*mk_radio(int x,int y,int w,int h){(void)w;(void)h;gtcaca_radiobutton_widget_t*r=gtcaca_radiobutton_new(NULL,"Small (Space)",1,x,y);gtcaca_radiobutton_set_active(r);gtcaca_radiobutton_new(NULL,"Large",1,x,y+1);return r;}
static void*mk_combo(int x,int y,int w,int h){(void)h;gtcaca_combobox_widget_t*c=gtcaca_combobox_new(NULL,x,y,w);gtcaca_combobox_append(c,"C");gtcaca_combobox_append(c,"Rust");gtcaca_combobox_append(c,"Go");return c;}
static void*mk_switch(int x,int y,int w,int h){(void)w;(void)h;gtcaca_switch_widget_t*s=gtcaca_switch_new(NULL,x,y);gtcaca_switch_set_active(s,1);return s;}
static void*mk_scale(int x,int y,int w,int h){(void)h;gtcaca_scale_widget_t*s=gtcaca_scale_new(NULL,x,y,w,0,100,5);gtcaca_scale_set_value(s,60);return s;}
static void*mk_spin(int x,int y,int w,int h){(void)w;(void)h;gtcaca_spinbutton_widget_t*s=gtcaca_spinbutton_new(NULL,x,y,0,100,1);gtcaca_spinbutton_set_value(s,42);return s;}
static void*mk_progress(int x,int y,int w,int h){(void)h;gtcaca_progressbar_widget_t*p=gtcaca_progressbar_new(NULL,x,y,w);gtcaca_progressbar_set_value(p,0.62f);return p;}
static void*mk_textlist(int x,int y,int w,int h){(void)w;(void)h;gtcaca_textlist_widget_t*t=gtcaca_textlist_new(NULL,x,y);
  static const char*it[]={"apple","banana","cherry","date","elderberry","fig","grape","kiwi","lemon","mango","nectarine","orange"};int i;
  for(i=0;i<12;i++)gtcaca_textlist_append(t,(char*)it[i]);
  gtcaca_textlist_widget_set_view_size(t,8);   /* show 8 of 12 (scrolls); also bounds the bg fill */
  return t;}
static void*mk_textview(int x,int y,int w,int h){gtcaca_textview_widget_t*t=gtcaca_textview_new(NULL,x,y,w,h);int i;for(i=1;i<=12;i++){char b[40];snprintf(b,sizeof b,"line %d — scroll me",i);gtcaca_textview_append(t,b);}return t;}
static void*mk_frame(int x,int y,int w,int h){gtcaca_frame_widget_t*f=gtcaca_frame_new(NULL,"Account",x,y,w,h);gtcaca_label_new(NULL,"name: ada",x+2,y+2);return f;}
static void*mk_sep(int x,int y,int w,int h){(void)h;return gtcaca_separator_new(NULL,x,y,w,0);}
static void*mk_expander(int x,int y,int w,int h){(void)h;gtcaca_expander_widget_t*e=gtcaca_expander_new(NULL,"Advanced options (Space)",x,y,w);
  gtcaca_expander_add_managed(e,GTCACA_WIDGET(gtcaca_label_new(NULL,"  - verbose logging",x+2,y+1)));
  gtcaca_expander_add_managed(e,GTCACA_WIDGET(gtcaca_label_new(NULL,"  - experimental features",x+2,y+2)));
  gtcaca_expander_set_expanded(e,0);   /* collapsed; Space reveals the managed labels */
  return e;}
static void noop_action(void*u){(void)u;}
static void*mk_menu(int x,int y,int w,int h){(void)x;(void)y;(void)w;(void)h;
  gtcaca_menu_widget_t*m=gtcaca_menu_new();
  int f=gtcaca_menu_add_entry(m,"File"); gtcaca_menu_add_item(m,f,"New","C-n",noop_action,NULL); gtcaca_menu_add_item(m,f,"Open","C-o",noop_action,NULL); gtcaca_menu_add_item(m,f,"Quit","C-q",noop_action,NULL);
  int e=gtcaca_menu_add_entry(m,"Edit"); gtcaca_menu_add_item(m,e,"Cut","C-w",noop_action,NULL); gtcaca_menu_add_item(m,e,"Copy","M-w",noop_action,NULL); gtcaca_menu_add_item(m,e,"Paste","C-y",noop_action,NULL);
  int h2=gtcaca_menu_add_entry(m,"Help"); gtcaca_menu_add_item(m,h2,"About",NULL,noop_action,NULL);
  return m;}
static void*mk_tabs(int x,int y,int w,int h){gtcaca_tabs_widget_t*t=gtcaca_tabs_new(NULL,x,y,w,h<3?3:h);gtcaca_tabs_set_title(t,"view");const char*ti[]={"Files","Edit","Build","Help"};gtcaca_tabs_set_titles(t,ti,4);return t;}
static void*mk_spark(int x,int y,int w,int h){gtcaca_sparkline_widget_t*s=gtcaca_sparkline_new(NULL,x,y,w,h);float v[28];int i;for(i=0;i<28;i++)v[i]=(float)(10+9*((i*7)%13));gtcaca_sparkline_set_data(s,v,28);return s;}
static void*mk_gauge(int x,int y,int w,int h){(void)h;gtcaca_gauge_widget_t*g=gtcaca_gauge_new(NULL,x,y,w);gtcaca_gauge_set_percent(g,72);return g;}
static void*mk_bar(int x,int y,int w,int h){gtcaca_barchart_widget_t*b=gtcaca_barchart_new(NULL,x,y,w,h);float v[5]={30,75,45,90,60};const char*l[5]={"Jan","Feb","Mar","Apr","May"};gtcaca_barchart_set_data(b,v,l,5);return b;}
static void*mk_line(int x,int y,int w,int h){gtcaca_linechart_widget_t*c=gtcaca_linechart_new(NULL,x,y,w,h);gtcaca_linechart_set_title(c,"sin / cos");double a[40],b[40];int i;for(i=0;i<40;i++){a[i]=sin(i*0.3);b[i]=0.6*cos(i*0.3);}gtcaca_linechart_add_series(c,a,40,CACA_LIGHTGREEN);gtcaca_linechart_add_series(c,b,40,CACA_LIGHTRED);return c;}
static void*mk_seg(int x,int y,int w,int h){gtcaca_segdisplay_widget_t*s=gtcaca_segdisplay_new(NULL,x,y,w,h);gtcaca_segdisplay_set_title(s,"clock");gtcaca_segdisplay_set_text(s,"12:45");return s;}
static void*mk_tree(int x,int y,int w,int h){gtcaca_tree_widget_t*t=gtcaca_tree_new(NULL,x,y,w,h);gtcaca_tree_set_title(t,"files");gtcaca_tree_set_model(t,&TREE);gtcaca_tree_key(t,CACA_KEY_RIGHT,NULL);return t;}
static void*mk_table(int x,int y,int w,int h){gtcaca_table_widget_t*t=gtcaca_table_new(NULL,x,y,w,h);gtcaca_table_set_title(t,"people");gtcaca_table_set_model(t,&TBL);gtcaca_table_set_current(t,0,0);return t;}
static void*mk_map(int x,int y,int w,int h){gtcaca_map_widget_t*m=gtcaca_map_new(NULL,x,y,w,h);gtcaca_map_set_title(m,"world");gtcaca_map_add_world(m,CACA_GREEN);const char*c[]={"London","New York","Tokyo","Sao Paulo","Sydney","Cairo"};int i;for(i=0;i<6;i++)gtcaca_map_add_city(m,c[i],'o',CACA_LIGHTGREEN);return m;}
static void*mk_cal(int x,int y,int w,int h){gtcaca_calendar_widget_t*c=gtcaca_calendar_new(NULL,x,y,w,h);gtcaca_calendar_set_date(c,2026,6,25);return c;}
static void*mk_mind(int x,int y,int w,int h){gtcaca_mindmap_widget_t*m=gtcaca_mindmap_new(NULL,x,y,w,h);gtcaca_mindmap_set_title(m,"plan");gtcaca_mindmap_set_text(m->root,"Project");
  gtcaca_mm_node_t*a=gtcaca_mindmap_add_child(m,m->root,"Design");gtcaca_mindmap_add_child(m,a,"UI");gtcaca_mindmap_add_child(m,a,"API");
  gtcaca_mm_node_t*b=gtcaca_mindmap_add_child(m,m->root,"Build");gtcaca_mindmap_add_child(m,b,"CI");gtcaca_mindmap_add_child(m,m->root,"Ship");gtcaca_mindmap_select(m,a);return m;}
static void*mk_editor(int x,int y,int w,int h){gtcaca_editor_widget_t*e=gtcaca_editor_new(NULL,x,y,w,h);gtcaca_editor_set_text(e,"int main(void) {\n    printf(\"hi\\n\");\n    return 0;\n}\n");return e;}
static void*mk_dialog(int x,int y,int w,int h){gtcaca_dialog_widget_t*d=gtcaca_dialog_new(NULL,x,y,w,h<6?6:h);const char*bb[]={"OK","Cancel"};gtcaca_dialog_set(d,"Confirm","Save changes before quitting?",bb,2);return d;}
static void*mk_fc(int x,int y,int w,int h){gtcaca_filechooser_widget_t*f=gtcaca_filechooser_new(NULL,x,y,w,h);gtcaca_filechooser_set_dir(f,".");return f;}

typedef struct { const char *name, *desc; int w, h; void *(*mk)(int,int,int,int); void *obj; } page_t;
static page_t P[] = {
  {"Label","static text",26,1,mk_label},
  {"Button","push button",10,3,mk_button},
  {"Entry","text input — type!",24,1,mk_entry},
  {"Checkbox","Space toggles",26,1,mk_check},
  {"Radio button","Space selects",22,2,mk_radio},
  {"Combo box","Up/Down/Enter",22,1,mk_combo},
  {"Switch","Space/Enter toggles",16,1,mk_switch},
  {"Scale","Left/Right adjusts",34,1,mk_scale},
  {"Spin button","Left/Right adjusts",16,1,mk_spin},
  {"Progress bar","determinate",34,1,mk_progress},
  {"Text list","Up/Down select",22,5,mk_textlist},
  {"Text view","Up/Down/PgDn scroll",34,8,mk_textview},
  {"Frame","grouping box",30,6,mk_frame},
  {"Separator","divider rule",30,1,mk_sep},
  {"Expander","Space expand/collapse",30,1,mk_expander},
  {"Tabs","Left/Right/Tab switch",44,3,mk_tabs},
  {"Sparkline","trend chart",36,6,mk_spark},
  {"Gauge","percentage bar",34,1,mk_gauge},
  {"Bar chart","labelled bars",36,9,mk_bar},
  {"Line chart","XY plot",52,12,mk_line},
  {"Seven-segment","LED digits",30,9,mk_seg},
  {"Tree","Right/Left fold, arrows",36,12,mk_tree},
  {"Table","arrows move the cell",36,9,mk_table},
  {"Map","arrows hop cities",62,17,mk_map},
  {"Calendar","arrows by day/week, PgUp/Dn",30,11,mk_cal},
  {"Mind map","arrows + / - fold",52,9,mk_mind},
  {"Editor","type to edit",42,8,mk_editor},
  {"Dialog","Left/Right + Enter",36,6,mk_dialog},
  {"File chooser","browse a directory",46,14,mk_fc},
  {"Menu","menu bar (drawn at the top)",1,1,mk_menu},
};
#define NP ((int)(sizeof P / sizeof P[0]))

static void draw_widget(void *o)
{
  switch (WTYPE(o)) {
  case GTCACA_WIDGET_LABEL: gtcaca_label_draw(o); break;
  case GTCACA_WIDGET_BUTTON: gtcaca_button_draw(o); break;
  case GTCACA_WIDGET_ENTRY: gtcaca_entry_draw(o); break;
  case GTCACA_WIDGET_CHECKBOX: gtcaca_checkbox_draw(o); break;
  case GTCACA_WIDGET_RADIOBUTTON: gtcaca_radiobutton_draw(o); break;
  case GTCACA_WIDGET_COMBOBOX: gtcaca_combobox_draw(o); break;
  case GTCACA_WIDGET_SWITCH: gtcaca_switch_draw(o); break;
  case GTCACA_WIDGET_SCALE: gtcaca_scale_draw(o); break;
  case GTCACA_WIDGET_SPINBUTTON: gtcaca_spinbutton_draw(o); break;
  case GTCACA_WIDGET_PROGRESSBAR: gtcaca_progressbar_draw(o); break;
  case GTCACA_WIDGET_TEXTLIST: gtcaca_textlist_draw(o); break;
  case GTCACA_WIDGET_TEXTVIEW: gtcaca_textview_draw(o); break;
  case GTCACA_WIDGET_FRAME: gtcaca_frame_draw(o); break;
  case GTCACA_WIDGET_SEPARATOR: gtcaca_separator_draw(o); break;
  case GTCACA_WIDGET_EXPANDER: {
    gtcaca_expander_widget_t *e = o; int i;
    gtcaca_expander_draw(e);
    for (i = 0; i < e->n_managed; i++)   /* show the managed labels when expanded */
      if (e->managed[i] && e->managed[i]->is_visible) gtcaca_label_draw((gtcaca_label_widget_t *)e->managed[i]);
    break;
  }
  case GTCACA_WIDGET_MENU: gtcaca_menu_draw(o); break;
  case GTCACA_WIDGET_TABS: gtcaca_tabs_draw(o); break;
  case GTCACA_WIDGET_SPARKLINE: gtcaca_sparkline_draw(o); break;
  case GTCACA_WIDGET_GAUGE: gtcaca_gauge_draw(o); break;
  case GTCACA_WIDGET_BARCHART: gtcaca_barchart_draw(o); break;
  case GTCACA_WIDGET_LINECHART: gtcaca_linechart_draw(o); break;
  case GTCACA_WIDGET_SEGDISPLAY: gtcaca_segdisplay_draw(o); break;
  case GTCACA_WIDGET_TREE: gtcaca_tree_draw(o); break;
  case GTCACA_WIDGET_TABLE: gtcaca_table_draw(o); break;
  case GTCACA_WIDGET_MAP: gtcaca_map_draw(o); break;
  case GTCACA_WIDGET_CALENDAR: gtcaca_calendar_draw(o); break;
  case GTCACA_WIDGET_MINDMAP: gtcaca_mindmap_draw(o); break;
  case GTCACA_WIDGET_EDITOR: gtcaca_editor_draw(o); break;
  case GTCACA_WIDGET_DIALOG: gtcaca_dialog_draw(o); break;
  case GTCACA_WIDGET_FILECHOOSERDIALOG: gtcaca_filechooser_draw(o); break;
  default: break;
  }
}

static void key_widget(void *o, int k)
{
  switch (WTYPE(o)) {
  case GTCACA_WIDGET_ENTRY:       ((gtcaca_entry_widget_t*)o)->private_key_cb(o,k,NULL); break;
  case GTCACA_WIDGET_BUTTON:      ((gtcaca_button_widget_t*)o)->private_key_cb(o,k,NULL); break;
  case GTCACA_WIDGET_CHECKBOX:    ((gtcaca_checkbox_widget_t*)o)->private_key_cb(o,k,NULL); break;
  case GTCACA_WIDGET_RADIOBUTTON: ((gtcaca_radiobutton_widget_t*)o)->private_key_cb(o,k,NULL); break;
  case GTCACA_WIDGET_COMBOBOX:    ((gtcaca_combobox_widget_t*)o)->private_key_cb(o,k,NULL); break;
  case GTCACA_WIDGET_TEXTLIST:    ((gtcaca_textlist_widget_t*)o)->private_key_cb(o,k,NULL); break;
  case GTCACA_WIDGET_TEXTVIEW:    ((gtcaca_textview_widget_t*)o)->private_key_cb(o,k,NULL); break;
  case GTCACA_WIDGET_MENU:        gtcaca_menu_handle_key(o,k); break;
  case GTCACA_WIDGET_EDITOR:      ((gtcaca_editor_widget_t*)o)->private_key_cb(o,k,NULL); break;
  case GTCACA_WIDGET_SWITCH:      if(k==' '||k==CACA_KEY_RETURN||k==10) gtcaca_switch_set_active(o,!gtcaca_switch_get_active(o)); break;
  case GTCACA_WIDGET_SCALE:       gtcaca_scale_handle_key(o,k); break;
  case GTCACA_WIDGET_SPINBUTTON:  gtcaca_spinbutton_handle_key(o,k); break;
  case GTCACA_WIDGET_EXPANDER:    gtcaca_expander_handle_key(o,k); break;
  case GTCACA_WIDGET_TREE:        gtcaca_tree_key(o,k,NULL); break;
  case GTCACA_WIDGET_TABLE:       gtcaca_table_key(o,k,NULL); break;
  case GTCACA_WIDGET_MAP:         gtcaca_map_key(o,k,NULL); break;
  case GTCACA_WIDGET_TABS:        gtcaca_tabs_key(o,k,NULL); break;
  case GTCACA_WIDGET_CALENDAR:    gtcaca_calendar_key(o,k,NULL); break;
  case GTCACA_WIDGET_MINDMAP:     gtcaca_mindmap_key(o,k,NULL); break;
  case GTCACA_WIDGET_DIALOG:      gtcaca_dialog_key(o,k,NULL); break;
  case GTCACA_WIDGET_FILECHOOSERDIALOG: gtcaca_filechooser_key(o,k,NULL); break;
  default: break;
  }
}

static int page_cmp(const void *a, const void *b)
{ return strcasecmp(((const page_t *)a)->name, ((const page_t *)b)->name); }

int main(int argc, char **argv)
{
  int sel = 0, W, H, LW = 22, rx, ry, rw, rh, i;
  int on_widget = 0;          /* 0 = list has focus, 1 = the widget */
  caca_event_t ev;

  gtcaca_init(&argc, &argv);
  gtcaca_application_new("gtcaca gallery");
  gmo.theme.textview.fg = CACA_LIGHTGRAY; gmo.theme.textview.bg = CACA_BLACK;
  gmo.theme.textviewfocus.fg = CACA_LIGHTGRAY; gmo.theme.textviewfocus.bg = CACA_BLACK;
  W = caca_get_canvas_width(gmo.cv); H = caca_get_canvas_height(gmo.cv);
  rx = LW + 2; ry = 2; rw = W - rx - 1; rh = H - ry - 2;

  qsort(P, NP, sizeof P[0], page_cmp);   /* list the widgets alphabetically */

  for (i = 0; i < NP; i++) {
    int cw = P[i].w < rw ? P[i].w : rw, ch = P[i].h < rh ? P[i].h : rh;
    P[i].obj = P[i].mk(rx, ry, cw, ch);
  }

  for (;;) {
    int x, k;
    caca_set_color_ansi(gmo.cv, CACA_LIGHTGRAY, CACA_BLACK); caca_clear_canvas(gmo.cv);
    /* header */
    caca_set_color_ansi(gmo.cv, CACA_BLACK, CACA_CYAN);
    for (x = 0; x < W; x++) caca_put_char(gmo.cv, x, 0, ' ');
    caca_printf(gmo.cv, 1, 0, " gtcaca gallery — %d widgets ", NP);
    /* left list */
    for (i = 0; i < NP && i < H - 2; i++) {
      int cur = (i == sel);
      if (cur && !on_widget) caca_set_color_ansi(gmo.cv, CACA_BLACK, CACA_CYAN);
      else if (cur)          caca_set_color_ansi(gmo.cv, CACA_WHITE, CACA_BLUE);
      else                   caca_set_color_ansi(gmo.cv, CACA_LIGHTGRAY, CACA_BLACK);
      caca_printf(gmo.cv, 0, 2 + i, " %-*.*s", LW, LW - 1, P[i].name);
    }
    for (i = 1; i < H - 1; i++) { caca_set_color_ansi(gmo.cv, CACA_DARKGRAY, CACA_BLACK); caca_put_char(gmo.cv, LW, i, 0x2502); }
    /* description + the widget */
    caca_set_color_ansi(gmo.cv, CACA_YELLOW, CACA_BLACK); caca_set_attr(gmo.cv, CACA_BOLD);
    caca_printf(gmo.cv, rx, 1, "%s", P[sel].name);
    caca_set_attr(gmo.cv, 0); caca_set_color_ansi(gmo.cv, CACA_LIGHTGRAY, CACA_BLACK);
    caca_printf(gmo.cv, rx + (int)strlen(P[sel].name) + 2, 1, "— %s", P[sel].desc);
    HFOCUS(P[sel].obj, on_widget);
    draw_widget(P[sel].obj);
    /* footer */
    caca_set_color_ansi(gmo.cv, CACA_BLACK, CACA_CYAN);
    for (x = 0; x < W; x++) caca_put_char(gmo.cv, x, H - 1, ' ');
    if (on_widget) caca_printf(gmo.cv, 1, H - 1, " using widget — keys go to it    Tab/Esc: back to list ");
    else           caca_printf(gmo.cv, 1, H - 1, " Up/Down: pick    Tab/Enter: use the widget    q: quit ");
    caca_refresh_display(gmo.dp);

    if (!caca_get_event(gmo.dp, CACA_EVENT_KEY_PRESS, &ev, -1)) continue;
    k = caca_get_event_key_ch(&ev);

    if (on_widget) {
      if (k == CACA_KEY_ESCAPE || (k == '\t' && WTYPE(P[sel].obj) != GTCACA_WIDGET_TABS &&
                                   WTYPE(P[sel].obj) != GTCACA_WIDGET_EDITOR))
        on_widget = 0;
      else
        key_widget(P[sel].obj, k);
    } else {
      if (k == 'q' || k == 'Q') break;
      else if (k == CACA_KEY_DOWN) sel = (sel + 1) % NP;
      else if (k == CACA_KEY_UP)   sel = (sel - 1 + NP) % NP;
      else if (k == '\t' || k == CACA_KEY_RETURN || k == 10 || k == CACA_KEY_RIGHT) on_widget = 1;
    }
  }
  caca_free_display(gmo.dp);
  return 0;
}
