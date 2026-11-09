/* Pixel-pair visibility for display text. Sprite commits overwrite text only
   inside their own rectangles, not everywhere inside the compositor union.
   Four bytes/cell track eight rows of four doubled AGI pixels. Stored in the
   unused tail of clean-picture page 39, already included in v1 snapshots.
   Old snapshots lack the marker and are initialized with fully visible text. */
#include <arch/zxn.h>
#include <string.h>
#include "runtime.h"
#include "platform.h"
#define MASKS ((uint8_t *)0x6900)
#define MARK ((char *)0x78a0)
#define TEXT_ROWS ((uint8_t *)0x78a4)
void rt_text_mask(uint8_t op,uint8_t x,uint8_t y,uint8_t w,uint8_t h) {
    uint8_t saved=ZXN_READ_REG(0x53),row,col,a,b,n,bits,attr,c,mask[4],ix,iy;
    uint16_t at;int16_t top,bottom,right,l,r,t,end;const uint8_t *font;uint8_t *dest;
    static const uint8_t caret[8]={0,16,40,68,0,0,0,0};
    ZXN_WRITE_REG(0x53,39);
    if(op==0 || memcmp(MARK,"TXT1",4)) {
        memset(MASKS,0,4000);memcpy(MARK,"TXT1",4);memset(TEXT_ROWS,0,4);
        /* Old v1 saves have cells but no visibility/row metadata. */
        if(op!=0)for(row=0;row<25;++row){
            ZXN_WRITE_REG(0x53,61);c=0;for(col=0;col<40;++col)c|=*((uint8_t *)(0x6000u+(uint16_t)row*40+col));
            ZXN_WRITE_REG(0x53,39);if(c)TEXT_ROWS[row>>3]|=1u<<(row&7);
        }
    }
    if(op==0 || op==4){memset((void *)0x7ff0,0,4);goto done;}
    if(op==5){for(row=x;row<25 && row<(uint16_t)x+y;++row)TEXT_ROWS[row>>3]&=~(1u<<(row&7));goto done;}
    if(op==1) {memset(MASKS+((uint16_t)y*40+x)*4,0,4);TEXT_ROWS[y>>3]|=1u<<(y&7);goto done;}
    if(op==2) {
        top=(int16_t)y-h+1+(uint16_t)rt.print_line*8;bottom=(uint16_t)y+1+(uint16_t)rt.print_line*8;
        right=(uint16_t)x+w;if(right>160)right=160;
        if(top<0)top=0;if(bottom>200)bottom=200;
        if(top>=bottom || x>=right)goto done;
        for(row=top/8;row<=(bottom-1)/8;++row) {
          ZXN_WRITE_REG(0x53,39);if(!(TEXT_ROWS[row>>3]&(1u<<(row&7))))continue;
          for(col=x/4;col<=(right-1)/4;++col) {
            at=(uint16_t)row*40+col;
            ZXN_WRITE_REG(0x53,61);c=*((uint8_t *)(0x6000u+at));
            if(!c)continue;
            l=(int16_t)x-col*4;if(l<0)l=0;r=right-col*4;if(r>4)r=4;
            bits=((1u<<r)-1)&~((1u<<l)-1);
            t=top-row*8;if(t<0)t=0;end=bottom-row*8;if(end>8)end=8;
            ZXN_WRITE_REG(0x53,39);at*=4;
            for(n=t;n<end;++n)MASKS[at+(n>>1)]|=bits<<((n&1)*4);
          }
        }
        goto done;
    }
    /* op 3: repaint a cell's surviving pixels; x/y are column/row, w/h
       are character/attribute. Fully covered cells cost no font rendering. */
    at=((uint16_t)y*40+x)*4;memcpy(mask,MASKS+at,4);
    a=mask[0]|mask[1]|mask[2]|mask[3];b=mask[0]&mask[1]&mask[2]&mask[3];
    if(b==255)goto done;
    if(!a){next_agi_char(x,y,w,h&15,h>>4);goto done;}
    c=w;attr=h;if(c<32 || c>127)c='?';if(c==96)c=39;
    font=(const uint8_t *)(0x3d00u+(uint16_t)(c-32)*8);if(c==94)font=caret;
    ZXN_WRITE_REG(0x53,40+(x>>2));dest=(uint8_t *)(0x6020u+((uint16_t)(x&3)<<11)+(uint16_t)y*8);
    for(ix=0;ix<8;++ix){for(iy=0;iy<8;++iy)if(!(mask[iy>>1]&(1u<<((ix>>1)+(iy&1)*4))))dest[iy]=font[iy]&(128>>ix)?attr&15:attr>>4;dest+=256;}
done:
    ZXN_WRITE_REG(0x53,saved);
}
