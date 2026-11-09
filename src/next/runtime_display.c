#include <arch/zxn.h>
#include <string.h>
#include "platform.h"
/* Private to overlay 55, shared with runtime_text.c. Modal backing uses
   32 KB on pages 58,59,62,63. Page 63 is PIC input scratch: decoding has
   finished before a modal UI opens, and the VM stays suspended until close.
   Menus pack the 16-colour viewport into 200 bytes per pair of columns.
   Normal dialogues instead save their 96 covered rows as full bytes. */
static void menu_transfer(uint16_t offset,uint8_t *buffer,uint16_t count,uint8_t save) {
    uint8_t page; uint16_t at,n;
    while(count) {
        page=offset>>13; at=offset&8191; n=8192-at;
        if(n>count) n=count;
        ZXN_WRITE_REG(0x53,58+page+(page>=2 ? 2 : 0));
        if(save) memcpy((void *)(0x6000u+at),buffer,n);
        else memcpy(buffer,(const void *)(0x6000u+at),n);
        offset+=n; buffer+=n; count-=n;
    }
}
static void menu_capture_region(uint8_t first,uint8_t last,uint8_t *scratch) {
    uint8_t pair,y,start,count,saved=ZXN_READ_REG(0x53); const uint8_t *pixels;
    if(first>last || last>=25) return;
    start=first*8; count=(last-first+1)*8;
    for(pair=0;pair<160;++pair) {
        ZXN_WRITE_REG(0x53,40+(pair>>4));
        pixels=(const uint8_t *)(0x6020u+((uint16_t)(pair&15)<<9)+start);
        for(y=0;y<count;++y) scratch[y]=(pixels[y]&15)|(pixels[256u+y]<<4);
        menu_transfer((uint16_t)pair*200+start,scratch,count,1);
    }
    ZXN_WRITE_REG(0x53,saved);
}
void next_menu_capture(uint8_t *scratch) { menu_capture_region(0,24,scratch); }
void next_menu_restore(uint8_t first,uint8_t last,uint8_t *scratch) {
    uint8_t pair,y,start,count,saved=ZXN_READ_REG(0x53); uint8_t *pixels;
    if(first>last || last>=25) return;
    start=first*8; count=(last-first+1)*8;
    /* Keep capture and restore loops separate: the current -SO3 toolchain
       miscompiles the second-column address when both share one function. */
    for(pair=0;pair<160;++pair) {
        menu_transfer((uint16_t)pair*200+start,scratch,count,0);
        ZXN_WRITE_REG(0x53,40+(pair>>4));
        pixels=(uint8_t *)(0x6020u+((uint16_t)(pair&15)<<9)+start);
        for(y=0;y<count;++y) { pixels[y]=scratch[y]&15; pixels[256u+y]=scratch[y]>>4; }
    }
    ZXN_WRITE_REG(0x53,saved);
}
static void message_region(uint8_t first,uint8_t last,uint8_t *scratch,uint8_t save) {
    uint8_t pair,saved,count=(last-first+1)*8; uint8_t *pixels;
    /* Wrapping may draw below the standard box. Pack those longer messages
       to stay within the same backing pages, including the bottom text row. */
    if(count>96) {
        if(save) menu_capture_region(first,last,scratch);
        else next_menu_restore(first,last,scratch);
        return;
    }
    saved=ZXN_READ_REG(0x53);
    for(pair=0;pair<160;++pair) {
        if(!save) menu_transfer((uint16_t)pair*192,scratch,192,0);
        ZXN_WRITE_REG(0x53,40+(pair>>4));
        pixels=(uint8_t *)(0x6020u+(uint16_t)first*8+((uint16_t)(pair&15)<<9));
        if(save) {
            memcpy(scratch,pixels,count); memcpy(scratch+96,pixels+256,count);
            menu_transfer((uint16_t)pair*192,scratch,192,1);
        } else { memcpy(pixels,scratch,count); memcpy(pixels+256,scratch+96,count); }
    }
    ZXN_WRITE_REG(0x53,saved);
}
void next_message_capture(uint8_t first,uint8_t last,uint8_t *scratch) { message_region(first,last,scratch,1); }
void next_message_restore(uint8_t first,uint8_t last,uint8_t *scratch) { message_region(first,last,scratch,0); }
/* Runtime AGI viewport: 320x200 centred vertically, 40x25 text cells. */
void next_screen_clear(uint8_t colour) {
    uint8_t p,saved=ZXN_READ_REG(0x53);
    for(p=40;p<50;++p) { ZXN_WRITE_REG(0x53,p); memset((void *)0x6000,colour&15,8192); }
    ZXN_WRITE_REG(0x53,saved);
}
void next_agi_char(uint8_t col,uint8_t row,uint8_t c,uint8_t fg,uint8_t bg) {
    static const uint8_t caret[8]={0,16,40,68,0,0,0,0};
    uint8_t x,y,mask=128,saved;const uint8_t *font;uint8_t *dest;
    if(col>=40 || row>=25) return;
    if(c<32 || c>127) c='?';
    if(c==96)c=39; /* AGI uses a backtick as an apostrophe; Spectrum ROM has GBP here. */
    saved=ZXN_READ_REG(0x53);
    /* Eight-pixel cells never cross a 32-column Layer 2 page. */
    ZXN_WRITE_REG(0x53,40+(col>>2));fg&=15;bg&=15;
    font=(const uint8_t *)(0x3d00u+(uint16_t)(c-32)*8);
    if(c==94)font=caret; /* Spectrum ROM has an up arrow, not an ASCII caret. */
    dest=(uint8_t *)(0x6020u+((uint16_t)(col&3)<<11)+(uint16_t)row*8);
    for(x=0;x<8;++x) {
        if(c==32)memset(dest,bg,8);
        else for(y=0;y<8;++y)dest[y]=font[y]&mask ? fg : bg;
        dest+=256;mask>>=1;
    }
    ZXN_WRITE_REG(0x53,saved);
}
