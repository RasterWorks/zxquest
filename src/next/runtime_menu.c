#include <string.h>
#include "runtime.h"
extern void next_menu_restore(uint8_t,uint8_t,uint8_t *);
extern void rt_frame(uint8_t,uint8_t,uint8_t,uint8_t,uint8_t);
static uint8_t previous_menu;
static uint8_t divider(void) {
    return strchr(rt_buffer,'-') && strspn(rt_buffer,"- ")==strlen(rt_buffer);
}
/* Return the label length without its authored alignment spaces. */
static uint8_t label_length(char *shortcut) {
    uint8_t n=shortcut ? shortcut-rt_buffer : strlen(rt_buffer);
    while(n && rt_buffer[n-1]==' ')--n;
    return n;
}
uint8_t rt_menu_draw(uint8_t *rows) {
    uint8_t i,j,n,label=0,keys=0,width=0,col=1,x=0,row=2,r=0,count=0;
    uint8_t fg=rt.fg,bg=rt.bg,changed=*rows==2 || previous_menu!=rt.menu_selected;
    char *shortcut,line[41];
    if(changed && *rows>2)next_menu_restore(1,*rows-1,(uint8_t *)rt_buffer);
    if(changed){rt.bg=15;strcpy(line," ");rt_text_at(0,0,line,0);}
    for(i=0;i<rt.menu_count && col<40;++i) {
        r=rt_format(rt.menus[i].logic,rt.menus[i].message,rt_buffer,sizeof(rt_buffer));if(r)goto done;
        if(i==rt.menu_selected)x=col-1;
        n=strlen(rt_buffer);if(n>40-col)n=40-col;
        rt_buffer[n]=0;
        if(changed) {
            rt.bg=i==rt.menu_selected?0:15;rt.fg=i==rt.menu_selected?15:0;
            rt_text_at(0,col,rt_buffer,0);
            rt.bg=15;strcpy(line,"  ");
            if(col+n<40) {line[40-col-n<2?40-col-n:2]=0;rt_text_at(0,col+n,line,0);}
        }
        col+=n+2;
    }
    if(changed && col<40) {
        memset(line,' ',40-col);line[40-col]=0;rt.bg=15;rt_text_at(0,col,line,0);
    }
    for(i=0;i<rt.item_count;++i)if(rt.items[i].menu==rt.menu_selected) {
        r=rt_format(rt.items[i].logic,rt.items[i].message,rt_buffer,sizeof(rt_buffer));if(r)goto done;
        ++count;if(divider())continue;
        shortcut=strchr(rt_buffer,'<');n=label_length(shortcut);if(n>label)label=n;
        if(shortcut){n=strlen(shortcut);if(n>keys)keys=n;}
    }
    width=label+(keys?keys+2:0);if(width>38)width=38;width+=2;
    if(x+width>40)x=40-width;
    if(changed)rt_frame(x,1,width,count+2,128);
    for(i=0;i<rt.item_count;++i)if(rt.items[i].menu==rt.menu_selected) {
        if(row>=23){r=VM_ARGUMENT;goto done;}
        r=rt_format(rt.items[i].logic,rt.items[i].message,rt_buffer,sizeof(rt_buffer));if(r)goto done;
        shortcut=strchr(rt_buffer,'<');n=label_length(shortcut);
        memset(line,' ',width);line[width]=0;
        for(j=0;j<n && j<width-2;++j)line[j+1]=rt_buffer[j];
        if(shortcut)for(j=width-1-strlen(shortcut);*shortcut && j<width-1;++j)line[j]=*shortcut++;
        if(divider())memset(line+1,'-',width-2);
        rt.fg=rt.items[i].enabled?(i==rt.item_selected?15:0):8;
        rt.bg=i==rt.item_selected?0:15;
        line[width-1]=0;rt_text_at(row++,x+1,line+1,0);
    }
    rt_frame(x,1,width,count+2,0);++row;
done:
    *rows=row;previous_menu=rt.menu_selected;rt.fg=fg;rt.bg=bg;return r;
}
