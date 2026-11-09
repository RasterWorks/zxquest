#include <string.h>
#include "runtime.h"
extern void rt_frame(uint8_t,uint8_t,uint8_t,uint8_t,uint8_t);
/* One layout routine measures and paints, so borders and backing always
   cover the same wrapped rows. No on-screen dismissal prompt is needed. */
static uint8_t flow(uint8_t width,uint8_t top,uint8_t left,uint8_t draw) {
    const char *s=rt_buffer;char line[39];uint8_t row=0,col=0;uint16_t word;
    memset(line,0,sizeof(line));
    while(*s && row<23) {
        if(*s=='\n' || *s=='\r') {
            if(draw){line[col]=0;rt_text_at(top+row,left,line,0);}
            if(*s++=='\r' && *s=='\n')++s;
            ++row;col=0;continue;
        }
        if(*s==' ') {++s;if(col<width)line[col++]=' ';continue;}
        for(word=0;s[word] && s[word]!=' ' && s[word]!='\n' && s[word]!='\r';++word){}
        if(col && word>width-col) {
            if(draw){line[col]=0;rt_text_at(top+row,left,line,0);}
            ++row;col=0;
        }
        while(word && row<23) {
            if(col==width){if(draw){line[col]=0;rt_text_at(top+row,left,line,0);}++row;col=0;if(row==23)break;}
            line[col++]=*s++;--word;
        }
    }
    if(row<23){if(draw){line[col]=0;rt_text_at(top+row,left,line,0);}++row;}
    return row;
}
uint16_t rt_dialog(uint8_t draw) {
    const char *s=rt_buffer;uint8_t width=1,n=0,rows,top,left,limit=36,fg=rt.fg,bg=rt.bg;
    /* Bit 1 requests print.at geometry. Width zero means 30 columns.
       The row anchors the box; the column anchors its first text column.
       Clamp the complete bordered box to our 40x25 character viewport. */
    if(draw&2){limit=rt.string_limit ? rt.string_limit : 30;if(limit>38)limit=38;}
    while(*s){if(*s=='\n' || *s=='\r')n=0;else if(n<limit)++n;if(n>width)width=n;++s;}
    rows=flow(width,0,0,0);top=(25-rows-2)/2;left=(40-width-2)/2;
    if(draw&2){top=rt.string_row;left=rt.string_col ? rt.string_col-1 : 0;
        if(top>23-rows)top=23-rows;if(left>38-width)left=38-width;}
    if(draw&1){rt_frame(left,top,width+2,rows+2,132);rt.fg=0;rt.bg=15;flow(width,top+1,left+1,1);rt.fg=fg;rt.bg=bg;}
    return ((uint16_t)top<<8)|(top+rows+1);
}
