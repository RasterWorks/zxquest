#ifndef NEXT_RUNTIME_H
#define NEXT_RUNTIME_H
#include "logic_resource.h"
#include "view_resource.h"
#define RT_KEYS 64
#define RT_MENUS 8
#define RT_ITEMS 48
#define RT_STRINGS 24
#define RT_STRING_SIZE 41
enum { RT_UI_NONE,RT_UI_MENU,RT_UI_MESSAGE,RT_UI_STRING,RT_UI_INVENTORY,RT_UI_OBJECT,RT_UI_SAVE,RT_UI_CONTROL };
enum { RT_MODAL_CAPTURE,RT_MODAL_RESTORE,RT_MODAL_CLEAR,RT_MODAL_FORMAT,RT_MODAL_PAGE };
enum { ACT_ANIMATED=1,ACT_DRAWN=2,ACT_UPDATE=4,ACT_CYCLING=8,
       ACT_FIXED_LOOP=16,ACT_FIXED_PRIORITY=32,ACT_IGNORE_HORIZON=64,ACT_IGNORE_OBJECTS=128 };
/* Secondary flags share the original ignore_blocks byte; records stay 32 bytes. */
enum { ACT_IGNORE_BLOCKS=1,ACT_ON_WATER=2,ACT_ON_LAND=4,ACT_REPOSITIONED=8,ACT_DIDNT_MOVE=16 };
enum { RT_MOTION_NORMAL,RT_MOTION_MOVE,RT_MOTION_FOLLOW,RT_MOTION_WANDER };
typedef struct {
    uint8_t x,y,view,loop,cel,width,height,priority,flags;
    uint8_t step,step_time,step_count,cycle_time,cycle_count,cycle,cycle_flag;
    uint8_t direction,motion,target_x,target_y,motion_flag,previous_step;
    uint8_t old_x,old_y,old_w,old_h,old_drawn;
    uint8_t ignore_blocks;
    uint8_t old_view,old_loop,old_cel,old_priority;
} rt_actor;
typedef struct { uint8_t ascii,scan,controller; } rt_binding;
typedef struct { uint8_t logic,message; } rt_menu;
typedef struct { uint8_t logic,message,menu,controller,enabled; } rt_item;
typedef struct {
    uint8_t horizon,player_control,input_enabled,gfx,picture_visible,picture_valid;
    uint8_t print_line,input_line,status_line,status_enabled,fg,bg,cursor;
    uint8_t controllers[32],pending[32],key,key_pending;
    uint8_t occupied[32],dirty;
    uint8_t locations[256]; uint16_t objects;
    char strings[RT_STRINGS][RT_STRING_SIZE],game_id[9];
    rt_binding keys[RT_KEYS]; uint8_t key_count;
    rt_menu menus[RT_MENUS]; uint8_t menu_count,menus_ready;
    rt_item items[RT_ITEMS]; uint8_t item_count;
    uint8_t ui,menu_selected,item_selected,string_id,string_limit,string_row,string_col;
    uint8_t trace_logic,trace_top,trace_height,trace_enabled,script_size;
    uint16_t elapsed_ms,second_frames,cycles,random;
    uint32_t sound_ticks; uint8_t sound_active,sound_flag,sound_fraction;
    uint8_t last_picture,error;
    uint8_t block_on,block[4];
} rt_state;
extern agi_vm rt_vm;
extern rt_state rt;
extern char rt_buffer[768];
uint8_t rt_init(const char *directory);
uint8_t rt_step(uint16_t frames);
void rt_key(uint8_t ascii,uint8_t scan);
uint8_t rt_command(agi_vm *v,uint8_t op,const uint8_t *p);
uint8_t rt_test(agi_vm *v,uint8_t op,const uint8_t *p,uint16_t at,uint8_t *value);
/* Stable resident callback addresses, including inside a renamed overlay. */
uint8_t rt_command_gate(agi_vm *v,uint8_t op,const uint8_t *p);
uint8_t rt_test_gate(agi_vm *v,uint8_t op,const uint8_t *p,uint16_t at,uint8_t *value);
uint8_t rt_actor_command(uint8_t op,const uint8_t *p);
uint8_t rt_actor_tick(void);
uint8_t rt_actor_render(void);
void rt_actor_reset(uint8_t initial);
uint8_t rt_actor_test(uint8_t op,const uint8_t *p,uint8_t *value);
void rt_motion_direction(uint8_t id,rt_actor *actor);
void rt_actor_read(uint8_t id,rt_actor *actor);
void rt_actor_write(uint8_t id,const rt_actor *actor);
uint8_t rt_text_command(uint8_t op,const uint8_t *p);
uint8_t rt_format(uint8_t logic,uint8_t message,char *out,uint16_t capacity);
uint8_t rt_ui_key(uint8_t ascii,uint8_t scan);
void rt_text_clear(void);
void rt_text_redraw(void);
void rt_text_at(uint8_t row,uint8_t col,const char *text,uint8_t permanent);
void rt_redraw(void);
uint8_t rt_picture(uint8_t id,uint8_t overlay);
uint8_t rt_objects(const char *directory);
uint8_t rt_object_name(uint8_t id,char *out); /* 41-byte resident destination */
uint8_t rt_inventory(uint8_t op,uint8_t a,uint8_t b);
uint8_t rt_modal(uint8_t op);
/* Stable resident callback address for the VIEW decoder in overlay 52. */
void rt_inventory_row(void *context,uint8_t row,const uint8_t *pixels,uint8_t width);
uint16_t next_clock(void);
void next_clock_start(void);
/* Frame-sampled key edges survive slow rendering; zero means queue empty. */
uint8_t next_key(void);
void next_key_sample(uint8_t key);
uint8_t rt_parser_init(const char *directory);
uint8_t rt_parser_command(uint8_t op,const uint8_t *p);
uint8_t rt_parser_test(uint8_t op,const uint8_t *p,uint16_t at,uint8_t *value);
uint8_t rt_parser_key(uint8_t ascii);
uint8_t rt_parser_cycle(uint8_t start);
uint8_t rt_parser_word(uint8_t number,char *out);
extern uint8_t rt_control_action,rt_paused,rt_joystick_mode,rt_joystick_owned;
uint8_t rt_control(uint8_t op,uint8_t key,uint8_t scan);
uint8_t rt_control_apply(void);
void rt_joystick(uint8_t raw);
void rt_joystick_config(uint8_t mode);
#endif
void rt_parser_invalidate_display(void);
