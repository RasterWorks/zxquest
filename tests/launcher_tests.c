#include <arch/zxn.h>
#include <arch/zxn/esxdos.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "next/runtime.h"
#include "next/platform.h"
extern uint8_t launcher_test(uint8_t,uint8_t,char *);
extern uint8_t rt_startup(void);
extern char cache_game_directory[];
/* Diagnostic NEX is mounted at the workspace SD root. */
static char root[]="/";static uint8_t file,checks,failures;
static void check(uint8_t ok,const char *name){++checks;if(!ok)++failures;sprintf(rt_buffer,"%s %s\n",ok?"PASS":"FAIL",name);esx_f_write(file,rt_buffer,strlen(rt_buffer));esx_f_sync(file);}
static void cd(const char *name){esx_f_chdir(root);strcpy(rt_buffer,"build/launcher-sd/");strcat(rt_buffer,name);esx_f_chdir(rt_buffer);}
static uint8_t scan(void){return launcher_test(0,0,rt_buffer);}
static uint8_t key(uint8_t k){return launcher_test(3,k,rt_buffer);}
static void cwd_normal(void){char *p;esx_f_getcwd(rt_buffer);for(p=rt_buffer;*p;++p)if(*p=='\\')*p='/';}
static void location(uint8_t r){char where[256];esx_f_getcwd(where);sprintf(rt_buffer,"LOCATION result=%u cwd=%s data=%s\n",r,where,cache_game_directory);esx_f_write(file,rt_buffer,strlen(rt_buffer));}
static void shot(void){uint8_t f,m=ZXN_READ_REG(0x53),i;strcpy(rt_buffer,"launcher-picker.l2");f=esx_f_open(rt_buffer,ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC);for(i=40;i<50;++i){ZXN_WRITE_REG(0x53,i);esx_f_write(f,(void *)0x6000,8192);}esx_f_close(f);ZXN_WRITE_REG(0x53,m);}
int main(void){
    uint8_t r;char cwd[101];next_init();next_clock_start();
    strcpy(rt_buffer,"build/launcher-tests.txt");file=esx_f_open(rt_buffer,ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC);
    cd("empty");check(scan()==0,"empty folder finds no games");
    cd("invalid");check(scan()==0,"incomplete files and invalid volume header are excluded");
    cd("direct");check(scan()==1,"game files in launch folder discovered");r=rt_startup();check(!r,"game in launch folder loads without a directory change");
    cd("stale");r=rt_startup();check(!r && !strcmp(cache_game_directory,"."),"flattened game loads despite stale nested CFG path");
    cd("single");check(scan()==1,"one child game discovered");r=rt_startup();location(r);cwd_normal();
    check(!r && strstr(rt_buffer,"single/SQ1") && !strcmp(cache_game_directory,"."),"one game loads automatically with saves anchored in its folder");
    cd("multi");check(scan()==3,"three child games discovered");
    launcher_test(1,0,cwd);check(!strcmp(cwd,"./KQ1"),"game folders sorted alphabetically");
    check(key(149)==1 && key(145)==0,"dedicated arrows change selection");
    check(key(11)==2 && key(10)==0,"classic arrows wrap at list ends");
    check(key(165)==1 && key(161)==0 && key(7)==0,"held-direction transitions work and unrelated keys leave selection alone");
    launcher_test(4,255,rt_buffer);shot();launcher_test(1,1,cwd);check(!strcmp(cwd,"./PQ1"),"drawing the picker preserves the selected path");
    launcher_test(5,149,rt_buffer);r=rt_startup();location(r);cwd_normal();
    check(!r && strstr(rt_buffer,"multi/PQ1"),"selected game loads in the matching save folder");
    cd("containers");check(scan()==1,"games container discovered");r=launcher_test(2,0,rt_buffer);cwd_normal();
    check(!r && strstr(rt_buffer,"containers/games/SQ1"),"container game loads with its own save folder");
    cd("legacy");check(scan()==1,"legacy child CFG resolves its nested game");r=launcher_test(2,0,rt_buffer);location(r);cwd_normal();
    check(!r && strstr(rt_buffer,"legacy/OLD") && !strcmp(cache_game_directory,"games/pq1"),"legacy game retains its existing save and resource paths");
    cd("long");check(scan()==1,"long folder names discovered");r=launcher_test(2,0,rt_buffer);check(!r,"long-name game loads");
    cd("explicit");r=rt_startup();esx_f_getcwd(rt_buffer);check(!r && strstr(rt_buffer,"explicit") && !strcmp(cache_game_directory,"../multi/PQ1"),"explicit CFG preserves existing launch behaviour");
    cd("auto");r=rt_startup();check(!r && !strcmp(cache_game_directory,"."),"AUTO configuration enables discovery");
    cd("many");r=scan();check(r==32 && launcher_test(1,0,cwd),"over-limit folder reports overflow instead of silently hiding games");
    check(key(145)==31 && key(149)==0,"selection wraps across menu pages");
    esx_f_chdir(root);sprintf(rt_buffer,"SUMMARY checks=%u failures=%u\n",checks,failures);esx_f_write(file,rt_buffer,strlen(rt_buffer));esx_f_close(file);for(;;){}
}
