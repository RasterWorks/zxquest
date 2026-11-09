#include <stdint.h>
#include <string.h>
#include "runtime.h"
static void replace(char *out,uint16_t capacity,const char *old,const char *value) {
    char *p=out;uint16_t a=strlen(old),b=strlen(value),n;
    while((p=strstr(p,old))!=0) {
        n=strlen(out);
        if(n-a+b>=capacity)return;
        memmove(p+b,p+a,strlen(p+a)+1);memcpy(p,value,b);p+=b;
    }
}
void rt_key_labels(char *out,uint16_t capacity) {
    uint8_t i;char from[4],to[6];char *source,*dest;
    replace(out,capacity,"F10","E0");
    for(i=1;i<=9;++i){from[0]='F';from[1]='0'+i;from[2]=0;
        to[0]='E';to[1]='0'+i;to[2]=0;replace(out,capacity,from,to);}
    replace(out,capacity,"Ctrl-J","E-J");replace(out,capacity,"Ctrl J","E-J");
    replace(out,capacity,"Alt-Z","E-Q");
    replace(out,capacity,"<Tab>","<INV>");
    replace(out,capacity,"   TAB shows the status screen.","INV VIDEO shows inventory.");
    replace(out,capacity,"   TAB shows status screen. (see below)","INV VIDEO shows inventory.");
    replace(out,capacity,"TAB shows the status screen.","INV VIDEO shows inventory.");
    if(!rt.gfx && rt_vm.logic==92 && !strcmp(rt.game_id,"SQ")) {
        source=out;while(*source==' ')++source;
        /* Keep the game's centred headings; only Help body lines align left. */
        if(!strcmp(source,"SPACE QUEST") || !strcmp(source,"Help"))return;
        
        replace(out,capacity,"ESC pops","BREAK pops");
        replace(out,capacity,"Ctrl-R toggles RGB monitor modes.","E5 = EDIT + 5; E1-E0 = F1-F10.\nTwo arrows = diagonal movement.");
        /* SQ1 displays Help at column one. Retain one leading space on
           each authored line, putting every line at screen column two. */
        dest=out;
        while(*dest) {
            source=dest;
            while(*source==' ')++source;
            memmove(dest,source,strlen(source)+1);
            if(!*dest || strlen(out)+1>=capacity)break;
            memmove(dest+1,dest,strlen(dest)+1);*dest=' ';
            dest=strchr(dest,'\n');if(!dest)break;++dest;
        }
    }
}
