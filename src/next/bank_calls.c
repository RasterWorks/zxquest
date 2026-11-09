/* Resident call gates. The old MMU4 mapping lives on the CPU stack, so calls
   from an overlay through a callback into another overlay can safely nest.
   All caller-owned buffers/state and callback entry points must be resident. */
#include <arch/zxn.h>
#include <string.h>
#include "agi/logic.h"
#include "logic_resource.h"
#define ENTER(page) uint8_t saved=ZXN_READ_REG(0x54); ZXN_WRITE_REG(0x54,page)
#define LEAVE() ZXN_WRITE_REG(0x54,saved)
extern uint8_t bank_next_key_decode(uint8_t,uint8_t,uint8_t,uint8_t,uint8_t,uint8_t);
uint8_t next_key_decode(uint8_t k,uint8_t a,uint8_t e,uint8_t l,uint8_t r,uint8_t x){uint8_t v;ENTER(52);v=bank_next_key_decode(k,a,e,l,r,x);LEAVE();return v;}
extern void bank_next_key_reset(void);
void next_key_reset(void){ENTER(52);bank_next_key_reset();LEAVE();}
extern uint16_t bank_rt_dialog(uint8_t);
uint16_t rt_dialog(uint8_t draw){uint16_t r;ENTER(52);r=bank_rt_dialog(draw);LEAVE();return r;}
extern void bank_rt_frame(uint8_t,uint8_t,uint8_t,uint8_t,uint8_t);
void rt_frame(uint8_t x,uint8_t y,uint8_t w,uint8_t h,uint8_t c){ENTER(52);bank_rt_frame(x,y,w,h,c);LEAVE();}
extern uint8_t bank_rt_menu_draw(uint8_t *);
uint8_t rt_menu_draw(uint8_t *rows){uint8_t r;ENTER(52);r=bank_rt_menu_draw(rows);LEAVE();return r;}
extern void bank_next_menu_restore(uint8_t,uint8_t,uint8_t *);
void next_menu_restore(uint8_t first,uint8_t last,uint8_t *scratch){ENTER(55);bank_next_menu_restore(first,last,scratch);LEAVE();}
extern void bank_rt_key_labels(char *,uint16_t);
void rt_key_labels(char *out,uint16_t capacity){ENTER(51);bank_rt_key_labels(out,capacity);LEAVE();}

extern void bank_agi_vm_init(agi_vm *,const agi_logic_io *,agi_command_fn,agi_test_fn);
extern void bank_agi_vm_stop(agi_vm *);
extern void bank_agi_vm_reset_scans(agi_vm *);
extern uint8_t bank_agi_vm_start(agi_vm *,uint8_t);
extern uint8_t bank_agi_vm_run(agi_vm *,uint16_t);
extern uint8_t bank_agi_vm_resume(agi_vm *);
extern uint8_t bank_agi_vm_flag(const agi_vm *,uint8_t);
extern void bank_agi_vm_set_flag(agi_vm *,uint8_t,uint8_t);
extern uint8_t bank_agi_vm_read_code(agi_vm *,uint16_t,void *,uint16_t);
extern uint8_t bank_agi_logic_header(const agi_logic_io *,uint32_t,uint16_t,uint16_t *);
extern uint8_t bank_agi_logic_message(const agi_logic_io *,uint32_t,uint16_t,uint8_t,uint16_t,char *,uint16_t,uint16_t *);
void agi_vm_init(agi_vm *v,const agi_logic_io *io,agi_command_fn c,agi_test_fn t) { ENTER(50); bank_agi_vm_init(v,io,c,t); LEAVE(); }
void agi_vm_stop(agi_vm *v) { ENTER(50); bank_agi_vm_stop(v); LEAVE(); }
void agi_vm_reset_scans(agi_vm *v) { ENTER(50); bank_agi_vm_reset_scans(v); LEAVE(); }
uint8_t agi_vm_start(agi_vm *v,uint8_t n) { uint8_t r; ENTER(50); r=bank_agi_vm_start(v,n); LEAVE(); return r; }
uint8_t agi_vm_run(agi_vm *v,uint16_t n) { uint8_t r; ENTER(50); r=bank_agi_vm_run(v,n); LEAVE(); return r; }
uint8_t agi_vm_resume(agi_vm *v) { uint8_t r; ENTER(50); r=bank_agi_vm_resume(v); LEAVE(); return r; }
uint8_t agi_vm_flag(const agi_vm *v,uint8_t n) { uint8_t r; ENTER(50); r=bank_agi_vm_flag(v,n); LEAVE(); return r; }
void agi_vm_set_flag(agi_vm *v,uint8_t n,uint8_t value) { ENTER(50); bank_agi_vm_set_flag(v,n,value); LEAVE(); }
uint8_t agi_vm_read_code(agi_vm *v,uint16_t at,void *b,uint16_t n) { uint8_t r; ENTER(50); r=bank_agi_vm_read_code(v,at,b,n); LEAVE(); return r; }
uint8_t agi_logic_header(const agi_logic_io *io,uint32_t h,uint16_t size,uint16_t *end) { uint8_t r; ENTER(50); r=bank_agi_logic_header(io,h,size,end); LEAVE(); return r; }
uint8_t agi_logic_message(const agi_logic_io *io,uint32_t h,uint16_t size,uint8_t n,uint16_t at,char *b,uint16_t cap,uint16_t *written) {
    uint8_t r; ENTER(50); r=bank_agi_logic_message(io,h,size,n,at,b,cap,written); LEAVE(); return r;
}

#include "resource_cache.h"
extern uint8_t bank_resource_scratch(uint8_t,uint8_t *);
extern void bank_resource_scratch_release(uint8_t,const uint8_t *);
uint8_t resource_scratch(uint8_t n,uint8_t *p) { uint8_t r;ENTER(53);r=bank_resource_scratch(n,p);LEAVE();return r; }
void resource_scratch_release(uint8_t n,const uint8_t *p) { ENTER(53);bank_resource_scratch_release(n,p);LEAVE(); }
extern uint8_t bank_resource_cache_init(const char *);
extern uint8_t bank_resource_acquire(uint8_t,uint8_t,resource_handle *);
extern uint8_t bank_resource_open(const char *,const char *,uint8_t,resource_stream *);
extern uint8_t bank_resource_read(resource_stream *,void *,uint16_t);
extern void bank_resource_close(resource_stream *);
extern uint8_t bank_resource_load(const char *,const char *,uint8_t,uint8_t *,uint16_t,uint16_t *);
uint8_t resource_cache_init(const char *d) { uint8_t r;ENTER(53);r=bank_resource_cache_init(d);LEAVE();return r; }
uint8_t resource_acquire(uint8_t t,uint8_t n,resource_handle *h) { uint8_t r;ENTER(53);r=bank_resource_acquire(t,n,h);LEAVE();return r; }
uint8_t resource_open(const char *d,const char *i,uint8_t n,resource_stream *s) { uint8_t r;ENTER(53);r=bank_resource_open(d,i,n,s);LEAVE();return r; }
uint8_t resource_read(resource_stream *s,void *d,uint16_t n) { uint8_t r;ENTER(53);r=bank_resource_read(s,d,n);LEAVE();return r; }
void resource_close(resource_stream *s) { ENTER(53);bank_resource_close(s);LEAVE(); }
uint8_t resource_load(const char *d,const char *i,uint8_t n,uint8_t *b,uint16_t c,uint16_t *l) { uint8_t r;ENTER(53);r=bank_resource_load(d,i,n,b,c,l);LEAVE();return r; }

extern uint8_t bank_next_logic_command(agi_vm *,uint8_t,const uint8_t *);
uint8_t next_logic_command(agi_vm *v,uint8_t op,const uint8_t *p){uint8_t r;ENTER(53);r=bank_next_logic_command(v,op,p);LEAVE();return r;}
extern void bank_resource_cache_status(resource_cache_info *);
void resource_cache_status(resource_cache_info *info){ENTER(53);bank_resource_cache_status(info);LEAVE();}

#ifndef BANKED_VM_ONLY
#include "runtime.h"
#include "sound.h"
extern uint8_t bank_rt_control(uint8_t,uint8_t,uint8_t);
uint8_t rt_control(uint8_t op,uint8_t key,uint8_t scan){uint8_t r;ENTER(95);r=bank_rt_control(op,key,scan);LEAVE();return r;}
extern uint8_t bank_rt_control_apply(void);
uint8_t rt_control_apply(void){uint8_t r;ENTER(95);r=bank_rt_control_apply();LEAVE();return r;}
extern void bank_rt_joystick(uint8_t);
void rt_joystick(uint8_t raw){ENTER(51);bank_rt_joystick(raw);LEAVE();}
extern void bank_rt_joystick_config(uint8_t);
void rt_joystick_config(uint8_t mode){ENTER(51);bank_rt_joystick_config(mode);LEAVE();}
extern void bank_next_sound_begin(const uint16_t *);
void next_sound_begin(const uint16_t *offsets){ENTER(56);bank_next_sound_begin(offsets);LEAVE();}
extern uint8_t bank_next_view_open(next_view *,uint8_t);
extern void bank_next_view_close(next_view *);
uint8_t next_view_open(next_view *v,uint8_t n){uint8_t r;ENTER(53);r=bank_next_view_open(v,n);LEAVE();return r;}
void next_view_close(next_view *v){ENTER(53);bank_next_view_close(v);LEAVE();}
extern uint8_t bank_rt_sound_start(uint8_t,uint8_t);
extern void bank_rt_sound_stop(void);
extern void bank_rt_sound_poll(void);
uint8_t rt_sound_start(uint8_t id,uint8_t flag) { uint8_t r;ENTER(56);r=bank_rt_sound_start(id,flag);LEAVE();return r; }
void rt_sound_stop(void) { ENTER(56);bank_rt_sound_stop();LEAVE(); }
void rt_sound_poll(void) { ENTER(56);bank_rt_sound_poll();LEAVE(); }
#include "platform.h"
#include "agi/picture.h"
#include "agi/view.h"
extern uint8_t bank_agi_picture_decode(const uint8_t *,uint16_t);
extern uint8_t bank_agi_view_inspect(const agi_view_source *,agi_view_info *);
extern uint8_t bank_agi_view_select(const agi_view_source *,uint8_t,uint8_t,agi_view_cel *);
extern uint8_t bank_agi_view_decode(const agi_view_source *,const agi_view_cel *,agi_view_row,void *);
extern uint8_t bank_agi_view_blit(const agi_view_source *,const agi_view_cel *,int16_t,int16_t,uint8_t);
extern uint8_t bank_agi_view_priority(uint8_t);
uint8_t agi_picture_decode(const uint8_t *b,uint16_t n) { uint8_t r; ENTER(51); r=bank_agi_picture_decode(b,n); LEAVE(); return r; }
uint8_t agi_view_inspect(const agi_view_source *s,agi_view_info *i) { uint8_t r; ENTER(93); r=bank_agi_view_inspect(s,i); LEAVE(); return r; }
uint8_t agi_view_select(const agi_view_source *s,uint8_t l,uint8_t c,agi_view_cel *i) { uint8_t r; ENTER(93); r=bank_agi_view_select(s,l,c,i); LEAVE(); return r; }
uint8_t agi_view_decode(const agi_view_source *s,const agi_view_cel *c,agi_view_row row,void *ctx) { uint8_t r; ENTER(93); r=bank_agi_view_decode(s,c,row,ctx); LEAVE(); return r; }
uint8_t agi_view_blit(const agi_view_source *s,const agi_view_cel *c,int16_t x,int16_t y,uint8_t p) { uint8_t r; ENTER(93); r=bank_agi_view_blit(s,c,x,y,p); LEAVE(); return r; }
uint8_t agi_view_priority(uint8_t y) { uint8_t r; ENTER(93); r=bank_agi_view_priority(y); LEAVE(); return r; }
extern void bank_next_scene_capture(void);
extern void bank_next_scene_restore(int16_t,int16_t,uint16_t,uint16_t);
extern void bank_next_show_rect(int16_t,int16_t,uint16_t,uint16_t,uint8_t);
void next_scene_capture(void) { ENTER(52); bank_next_scene_capture(); LEAVE(); }
void next_scene_restore(int16_t x,int16_t y,uint16_t w,uint16_t h) { ENTER(52); bank_next_scene_restore(x,y,w,h); LEAVE(); }
void next_show_rect(int16_t x,int16_t y,uint16_t w,uint16_t h,uint8_t p) { ENTER(52); bank_next_show_rect(x,y,w,h,p); LEAVE(); }
extern uint8_t bank_rt_init(const char *);
extern uint8_t bank_rt_step(uint16_t);
extern void bank_rt_key(uint8_t,uint8_t);
extern uint8_t bank_rt_command(agi_vm *,uint8_t,const uint8_t *);
extern uint8_t bank_rt_test(agi_vm *,uint8_t,const uint8_t *,uint16_t,uint8_t *);
uint8_t rt_init(const char *d) { uint8_t r; ENTER(53); r=bank_rt_init(d); LEAVE(); return r; }
uint8_t rt_step(uint16_t n) { uint8_t r; ENTER(53); r=bank_rt_step(n); LEAVE(); return r; }
void rt_key(uint8_t a,uint8_t s) { ENTER(53); bank_rt_key(a,s); LEAVE(); }
uint8_t rt_command(agi_vm *v,uint8_t op,const uint8_t *p) { uint8_t r; ENTER(53); r=bank_rt_command(v,op,p); LEAVE(); return r; }
uint8_t rt_test(agi_vm *v,uint8_t op,const uint8_t *p,uint16_t at,uint8_t *value) { uint8_t r; ENTER(53); r=bank_rt_test(v,op,p,at,value); LEAVE(); return r; }
uint8_t rt_command_gate(agi_vm *v,uint8_t op,const uint8_t *p) { return rt_command(v,op,p); }
uint8_t rt_test_gate(agi_vm *v,uint8_t op,const uint8_t *p,uint16_t at,uint8_t *value) { return rt_test(v,op,p,at,value); }
extern void bank_rt_actor_read(uint8_t,rt_actor *);
extern void bank_rt_actor_write(uint8_t,const rt_actor *);
extern uint8_t bank_rt_picture(uint8_t,uint8_t);
extern uint8_t bank_rt_objects(const char *);
extern void bank_rt_redraw(void);
void rt_actor_read(uint8_t id,rt_actor *a) { ENTER(53); bank_rt_actor_read(id,a); LEAVE(); }
void rt_actor_write(uint8_t id,const rt_actor *a) { ENTER(53); bank_rt_actor_write(id,a); LEAVE(); }
uint8_t rt_picture(uint8_t id,uint8_t overlay) { uint8_t r; ENTER(53); r=bank_rt_picture(id,overlay); LEAVE(); return r; }
uint8_t rt_objects(const char *d) { uint8_t r; ENTER(95); r=bank_rt_objects(d); LEAVE(); return r; }
void rt_redraw(void) { ENTER(53); bank_rt_redraw(); LEAVE(); }
extern uint8_t bank_rt_actor_command(uint8_t,const uint8_t *);
extern uint8_t bank_rt_actor_tick(void);
extern uint8_t bank_rt_actor_render(void);
extern void bank_rt_actor_reset(uint8_t);
extern uint8_t bank_rt_actor_test(uint8_t,const uint8_t *,uint8_t *);
uint8_t rt_actor_command(uint8_t op,const uint8_t *p) { uint8_t r; ENTER(54); r=bank_rt_actor_command(op,p); LEAVE(); return r; }
uint8_t rt_actor_tick(void) { uint8_t r; ENTER(54); r=bank_rt_actor_tick(); LEAVE(); return r; }
uint8_t rt_actor_render(void) { uint8_t r; ENTER(93); r=bank_rt_actor_render(); LEAVE(); return r; }
void rt_actor_reset(uint8_t n) { ENTER(54); bank_rt_actor_reset(n); LEAVE(); }
uint8_t rt_actor_test(uint8_t op,const uint8_t *p,uint8_t *value) { uint8_t r; ENTER(54); r=bank_rt_actor_test(op,p,value); LEAVE(); return r; }
extern uint8_t bank_rt_object_name(uint8_t,char *);
extern uint8_t bank_rt_inventory(uint8_t,uint8_t,uint8_t);
extern void bank_inventory_row(void *,uint8_t,const uint8_t *,uint8_t);
extern uint8_t bank_rt_modal(uint8_t);
uint8_t rt_object_name(uint8_t id,char *out) { uint8_t r;ENTER(95);r=bank_rt_object_name(id,out);LEAVE();return r; }
uint8_t rt_inventory(uint8_t op,uint8_t a,uint8_t b) { uint8_t r;ENTER(95);r=bank_rt_inventory(op,a,b);LEAVE();return r; }
void rt_inventory_row(void *ctx,uint8_t y,const uint8_t *p,uint8_t w) { ENTER(95);bank_inventory_row(ctx,y,p,w);LEAVE(); }
uint8_t rt_modal(uint8_t op) { uint8_t r;ENTER(55);r=bank_rt_modal(op);LEAVE();return r; }
extern void bank_rt_motion_direction(uint8_t,rt_actor *);
void rt_motion_direction(uint8_t id,rt_actor *a) { ENTER(95); bank_rt_motion_direction(id,a); LEAVE(); }
extern uint8_t bank_rt_text_command(uint8_t,const uint8_t *);
extern uint8_t bank_rt_format(uint8_t,uint8_t,char *,uint16_t);
extern uint8_t bank_rt_ui_key(uint8_t,uint8_t);
extern void bank_rt_text_clear(void);
extern void bank_rt_text_redraw(void);
extern void bank_rt_text_at(uint8_t,uint8_t,const char *,uint8_t);
uint8_t rt_text_command(uint8_t op,const uint8_t *p) { uint8_t r; ENTER(55); r=bank_rt_text_command(op,p); LEAVE(); return r; }
uint8_t rt_format(uint8_t l,uint8_t m,char *s,uint16_t n) { uint8_t r; ENTER(55); r=bank_rt_format(l,m,s,n); LEAVE(); return r; }
uint8_t rt_ui_key(uint8_t a,uint8_t s) { uint8_t r; ENTER(55); r=bank_rt_ui_key(a,s); LEAVE(); return r; }
void rt_text_clear(void) { ENTER(55); bank_rt_text_clear(); LEAVE(); }
void rt_text_redraw(void) { ENTER(55); bank_rt_text_redraw(); LEAVE(); }
void rt_text_at(uint8_t row,uint8_t col,const char *s,uint8_t permanent) { ENTER(55); bank_rt_text_at(row,col,s,permanent); LEAVE(); }
extern uint8_t bank_rt_save_slot(uint8_t,uint8_t);
extern uint8_t bank_rt_shake(uint8_t);
uint8_t rt_shake(uint8_t n) { uint8_t r;ENTER(93);r=bank_rt_shake(n);LEAVE();return r; }
extern uint8_t bank_rt_save_description(uint8_t,char *);
extern uint8_t bank_rt_save_ui(uint8_t,uint8_t,uint8_t);
extern void bank_rt_parser_reset_input(void);
extern uint8_t bank_rt_startup(void);
uint8_t rt_save_slot(uint8_t slot,uint8_t restore) { uint8_t r;ENTER(92);r=bank_rt_save_slot(slot,restore);LEAVE();return r; }
uint8_t rt_save_description(uint8_t slot,char *out) { uint8_t r;ENTER(92);r=bank_rt_save_description(slot,out);LEAVE();return r; }
uint8_t rt_save_ui(uint8_t op,uint8_t a,uint8_t s) { uint8_t r;ENTER(51);r=bank_rt_save_ui(op,a,s);LEAVE();return r; }
void rt_parser_reset_input(void) { ENTER(56);bank_rt_parser_reset_input();LEAVE(); }
uint8_t rt_startup(void) { uint8_t r;ENTER(51);r=bank_rt_startup();LEAVE();return r; }
extern void bank_next_screen_clear(uint8_t);
extern void bank_next_agi_char(uint8_t,uint8_t,uint8_t,uint8_t,uint8_t);
void next_screen_clear(uint8_t c) { ENTER(55); bank_next_screen_clear(c); LEAVE(); }
void next_agi_char(uint8_t col,uint8_t row,uint8_t c,uint8_t fg,uint8_t bg) { ENTER(55); bank_next_agi_char(col,row,c,fg,bg); LEAVE(); }
extern uint8_t bank_rt_parser_init(const char *);
extern uint8_t bank_rt_parser_command(uint8_t,const uint8_t *);
extern uint8_t bank_rt_parser_test(uint8_t,const uint8_t *,uint16_t,uint8_t *);
extern uint8_t bank_rt_parser_key(uint8_t);
extern void bank_rt_parser_invalidate_display(void);
void rt_parser_invalidate_display(void){ENTER(56);bank_rt_parser_invalidate_display();LEAVE();}
extern uint8_t bank_rt_parser_cycle(uint8_t);
extern uint8_t bank_rt_parser_word(uint8_t,char *);
uint8_t rt_parser_init(const char *s){uint8_t r;ENTER(56);r=bank_rt_parser_init(s);LEAVE();return r;}
uint8_t rt_parser_command(uint8_t op,const uint8_t *p){uint8_t r;ENTER(56);r=bank_rt_parser_command(op,p);LEAVE();return r;}
uint8_t rt_parser_test(uint8_t op,const uint8_t *p,uint16_t at,uint8_t *v){uint8_t r;ENTER(56);r=bank_rt_parser_test(op,p,at,v);LEAVE();return r;}
uint8_t rt_parser_key(uint8_t a){uint8_t r;ENTER(56);r=bank_rt_parser_key(a);LEAVE();return r;}
uint8_t rt_parser_cycle(uint8_t a){uint8_t r;ENTER(56);r=bank_rt_parser_cycle(a);LEAVE();return r;}
uint8_t rt_parser_word(uint8_t n,char *s){uint8_t r;ENTER(56);r=bank_rt_parser_word(n,s);LEAVE();return r;}

extern void bank_rt_host_key(uint8_t);
extern void bank_rt_stop_message(uint8_t);
void rt_host_key(uint8_t k){ENTER(53);bank_rt_host_key(k);LEAVE();}
void rt_stop_message(uint8_t r){ENTER(53);bank_rt_stop_message(r);LEAVE();}


extern void bank_next_init(void);
extern void bank_next_show_picture(uint8_t);
extern void bank_next_text(uint8_t,uint8_t,const char *);
extern void bank_next_clock_start(void);
void next_init(void){ENTER(52);bank_next_init();LEAVE();}
void next_show_picture(uint8_t p){ENTER(52);bank_next_show_picture(p);LEAVE();}
void next_text(uint8_t x,uint8_t y,const char *s){
    char text[41];uint8_t saved;
    /* Caller literals may live in its code overlay. Stage before switching. */
    strncpy(text,s,40);text[40]=0;saved=ZXN_READ_REG(0x54);ZXN_WRITE_REG(0x54,55);
    bank_next_text(x,y,text);ZXN_WRITE_REG(0x54,saved);
}
void next_clock_start(void){ENTER(52);bank_next_clock_start();LEAVE();}
extern void bank_rt_text_mask(uint8_t,uint8_t,uint8_t,uint8_t,uint8_t);
void rt_text_mask(uint8_t op,uint8_t x,uint8_t y,uint8_t w,uint8_t h){ENTER(56);bank_rt_text_mask(op,x,y,w,h);LEAVE();}
#endif
