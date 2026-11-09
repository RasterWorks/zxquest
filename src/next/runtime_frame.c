#include <stdint.h>
#include <string.h>
#include <arch/zxn.h>
/* Cell bounds, one pixel inset line and white outer margin. Bit 7 fills
   the interior; subsequent border-only calls preserve text and selection. */
void rt_frame(uint8_t col,uint8_t row,uint8_t width,uint8_t height,uint8_t colour) {
    uint16_t x,left=(uint16_t)col*8,right=left+(uint16_t)width*8;
    uint8_t top=32+row*8,n=height*8,saved=ZXN_READ_REG(0x53),*p;
    if(!width || !height || col+width>40 || row+height>25)return;
    for(x=left;x<right;++x) {
        ZXN_WRITE_REG(0x53,40+(x>>5));p=(uint8_t *)(0x6000u+((x&31)<<8)+top);
        if(colour&128)memset(p,15,n);
        if(x==left+2 || x==right-3)memset(p+2,colour&15,n-4);
        else if(x>left+2 && x<right-3){p[2]=colour&15;p[n-3]=colour&15;}
    }
    ZXN_WRITE_REG(0x53,saved);
}
