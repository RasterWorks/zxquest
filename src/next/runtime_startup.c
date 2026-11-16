/* Launch-folder discovery uses code page 92 and startup scratch page 58, outside snapshots. */
#include <arch/zxn/esxdos.h>
#include <arch/zxn.h>
#include <errno.h>
#include <string.h>
#include "runtime.h"
#include "platform.h"
#define GAME_LIMIT 32
typedef struct {char base[101],data[101];} launch_game;
static uint8_t games,selected,overflow,container;
#ifdef STARTUP_TEST
#include <intrinsic.h>
static uint8_t test_key;
extern void next_key_sample(uint8_t);
#endif
static void entry(uint8_t i,launch_game *g,uint8_t write){
    uint8_t m=ZXN_READ_REG(0x53);void *p=(void *)(0x6000u+(uint16_t)i*sizeof(*g));
    ZXN_WRITE_REG(0x53,58);if(write)memcpy(p,g,sizeof(*g));else memcpy(g,p,sizeof(*g));ZXN_WRITE_REG(0x53,m);
}
/* Strings passed to the OS or another code overlay must be resident. */
static uint8_t config(const char *base,char *out){
    uint8_t f;uint16_t n;
    strcpy(rt_buffer,base);strcat(rt_buffer,"/ZXQUEST.CFG");errno=0;
    f=esx_f_open(rt_buffer,ESX_MODE_READ|ESX_MODE_OPEN_EXIST);if(errno)return 0;
    errno=0;n=esx_f_read(f,out,100);esx_f_close(f);if(errno || !n || n==100)return 0;
    while(n && (out[n-1]=='\r' || out[n-1]=='\n'))--n;out[n]=0;
    return n && strcmp(out,"AUTO") && strcmp(out,"auto");
}
static uint8_t valid(const char *path){
    static const char *names[]={"LOGDIR","PICDIR","VIEWDIR","SNDDIR","OBJECT","WORDS.TOK"};
    uint8_t i,f,b,r;resource_stream s;char index[7];
    for(i=0;i<6;++i){
        strcpy(rt_buffer,path);strcat(rt_buffer,"/");strcat(rt_buffer,names[i]);errno=0;
        f=esx_f_open(rt_buffer,ESX_MODE_READ|ESX_MODE_OPEN_EXIST);if(errno)return 0;
        r=esx_f_read(f,&b,1)==1 && !errno;esx_f_close(f);if(!r)return 0;
    }
    strcpy(index,"LOGDIR");strcpy(rt_buffer,path);
    if(resource_open(rt_buffer,index,0,&s))return 0;
    b=s.length>=3;resource_close(&s);return b;
}
static uint8_t compare(const char *a,const char *b){
    uint8_t x,y;do{x=*a++;y=*b++;if(x>='a'&&x<='z')x-=32;if(y>='a'&&y<='z')y-=32;if(x!=y)return x<y?0:2;}while(x);return 1;
}
static void candidate(const char *base){
    /* Upper scratch holds the candidate, avoiding a large nested CPU stack. */
    launch_game *g=(launch_game *)(rt_buffer+512),*previous=(launch_game *)(rt_buffer+256);char joined[101];uint8_t i;
    strcpy(g->base,base);strcpy(g->data,".");
    if(!valid(base)){
        if(!config(base,g->data) || g->data[0]=='/' || g->data[0]=='\\' || strchr(g->data,':') || strstr(g->data,".."))return;
        if(strlen(base)+strlen(g->data)+1>100)return;
        strcpy(joined,base);strcat(joined,"/");strcat(joined,g->data);if(!valid(joined))return;
    }
    if(games==GAME_LIMIT){overflow=1;return;}
    i=games;
    while(i){entry(i-1,previous,0);if(compare(previous->base,g->base)!=2)break;entry(i,previous,1);--i;}
    entry(i,g,1);++games;
}
static void scan_folder(const char *base){
    uint8_t d,r;char path[101];struct esx_dirent_lfn *e=(struct esx_dirent_lfn *)rt_buffer;
    /* CSpect opens only the current directory. Open there, then restore the
       launch directory before validating entries. The handle retains its directory.
       The only bases used here are "." and its immediate child "games". */
    if(strcmp(base,".")){strcpy(path,base);errno=0;esx_f_chdir(path);if(errno)return;}
    strcpy(path,".");errno=0;d=esx_f_opendir_ex(path,ESX_DIR_USE_LFN);
    if(errno){errno=0;d=esx_f_opendir(path);}r=errno;
    if(strcmp(base,".")){strcpy(path,"..");esx_f_chdir(path);}
    if(r)return;
    for(;;){
        errno=0;r=esx_f_readdir(d,e);if(errno || !r)break;
        if(!(e->attr&ESX_DIR_A_DIR) || e->name[0]=='.')continue;
        if(base[0]=='.' && compare((char *)e->name,"games")==1)container=1;
        if(strlen(base)+strlen((char *)e->name)+1>100)continue;
        strcpy(path,base);strcat(path,"/");strcat(path,(char *)e->name);
        candidate(path);if(overflow)break;
    }
    esx_f_closedir(d);
}
static uint8_t discover(void){
    games=selected=overflow=container=0;
    if(valid("."))candidate(".");
    scan_folder(".");if(container && !overflow)scan_folder("games");
    return games;
}
static uint8_t choose_key(uint8_t key){
    if(!games)return 0;
    if(key==11 || key==145 || key==161)selected=selected?selected-1:games-1;
    if(key==10 || key==149 || key==165)selected=selected+1==games?0:selected+1;
    return key==13;
}
static void picker(uint8_t old){
    launch_game g;uint8_t i,first=(selected/18)*18,clear=old==255 || old/18!=selected/18;char *name;
    if(clear){next_screen_clear(0);strcpy(rt_buffer,"ZXQUEST");next_text(16,2,rt_buffer);
        strcpy(rt_buffer,"CHOOSE A GAME");next_text(13,4,rt_buffer);}
    for(i=first;i<games && i<first+18;++i){
        if(!clear && i!=old && i!=selected)continue;
        entry(i,&g,0);name=g.base;if(name[0]=='.' && name[1]=='/')name+=2;
        memset(rt_buffer,' ',39);rt_buffer[39]=0;rt_buffer[0]=i==selected?'>':' ';
        strncpy(rt_buffer+2,!strcmp(name,".")?"Current folder":name,36);rt_buffer[38]=0;
        next_text(1,6+i-first,rt_buffer);
    }
    if(clear){strcpy(rt_buffer,"Arrows: select    Enter: load");next_text(5,26,rt_buffer);}
}
static uint8_t load(uint8_t i){
    launch_game g;entry(i,&g,0);errno=0;if(strcmp(g.base,"."))esx_f_chdir(g.base);if(errno)return VM_IO;
    next_screen_clear(0);strcpy(rt_buffer,"LOADING ORIGINAL GAME FILES");next_text(0,5,rt_buffer);
    return rt_init(g.data);
}
uint8_t rt_startup(void){
    char path[101];uint8_t key,old;
    if(ZXN_READ_REG(0x00)==10 && (ZXN_READ_REG(0x01)>0x31 ||
       (ZXN_READ_REG(0x01)==0x31 && ZXN_READ_REG(0x0e)>=5)))ZXN_WRITE_REG(0x68,ZXN_READ_REG(0x68)|16);
    next_clock_start();
    /* A moved/flattened game can leave a stale CFG beside otherwise valid data. */
    if(config(".",path) && valid(path)){
        strcpy(rt_buffer,"LOADING ORIGINAL GAME FILES");next_text(0,5,rt_buffer);return rt_init(path);
    }
    strcpy(rt_buffer,"LOOKING FOR AGI GAMES...");next_text(3,5,rt_buffer);discover();
    if(!games || overflow){
        next_screen_clear(0);strcpy(rt_buffer,overflow?"TOO MANY GAMES (MAXIMUM 32)":"NO AGI V2 GAMES FOUND");next_text(2,8,rt_buffer);
        strcpy(rt_buffer,"Put each game's files in a subfolder.");next_text(1,11,rt_buffer);
        strcpy(rt_buffer,"LOGDIR PICDIR VIEWDIR SNDDIR");next_text(3,14,rt_buffer);
        strcpy(rt_buffer,"OBJECT WORDS.TOK and VOL files");next_text(3,16,rt_buffer);return VM_IO;
    }
    if(games>1){
        picker(255);
#ifdef STARTUP_TEST
        if(test_key){intrinsic_di();next_key_sample(test_key);next_key_sample(0);next_key_sample(13);test_key=0;intrinsic_ei();}
#endif
        for(;;){key=next_key();if(!key)continue;old=selected;if(choose_key(key))break;if(selected!=old)picker(old);}
    }
    return load(selected);
}
#ifdef STARTUP_TEST
uint8_t startup_test(uint8_t op,uint8_t arg,char *out){
    launch_game *g=(launch_game *)(rt_buffer+512);
    if(!op)return discover();
    if(op==1){entry(arg,g,0);strcpy(out,g->base);return overflow;}
    if(op==2)return load(arg);
    if(op==3){choose_key(arg);return selected;}
    if(op==4){picker(arg);return selected;}
    if(op==5){test_key=arg;return 0;}
    return config(".",out);
}
#endif
