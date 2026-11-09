#include <arch/zxn.h>
#include <arch/zxn/esxdos.h>
#include <stdio.h>
#include <string.h>
#include "next/runtime.h"
#include "next/platform.h"
static uint8_t file,checks,failures;
static void check(uint8_t ok,const char *s) {
    ++checks;if(!ok)++failures;sprintf(rt_buffer,"%s %s\n",ok?"PASS":"FAIL",s);
    esx_f_write(file,rt_buffer,strlen(rt_buffer));
}
static uint8_t command(uint8_t op,uint8_t a) {
    uint8_t p[7]={0};p[0]=a;return rt_command(&rt_vm,op,p);
}
static const uint16_t crc_table[16]={0,0x1021,0x2042,0x3063,0x4084,0x50a5,0x60c6,0x70e7,
    0x8108,0x9129,0xa14a,0xb16b,0xc18c,0xd1ad,0xe1ce,0xf1ef};
static uint16_t checksum(uint8_t first,uint8_t pages) {
    uint16_t crc=0xffff,at;uint8_t i,b,saved=ZXN_READ_REG(0x53);
    for(i=0;i<pages;++i) {
        ZXN_WRITE_REG(0x53,first+i);
        for(at=0;at<8192;++at){b=*((uint8_t *)(0x6000u+at));crc=(crc<<4)^crc_table[(crc>>12)^(b>>4)];crc=(crc<<4)^crc_table[(crc>>12)^(b&15)];}
    }
    ZXN_WRITE_REG(0x53,saved);return crc;
}
static void screen(const char *name) {
    uint8_t f,page,saved=ZXN_READ_REG(0x53);uint16_t at;char path[64];strcpy(path,name);
    f=esx_f_open(path,ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC);
    for(page=40;page<50;++page)for(at=0;at<8192;at+=512){ZXN_WRITE_REG(0x53,page);memcpy(rt_buffer,(const void *)(0x6000u+at),512);esx_f_write(f,rt_buffer,512);}
    esx_f_close(f);ZXN_WRITE_REG(0x53,saved);
}
static uint8_t scan(void) {
    uint8_t r;uint16_t n=0;
    do{r=rt_step(n?0:5);++n;}while(r==VM_RUNNING&&n<500);return r;
}
static void sentence(const char *s) {while(*s)rt_key(*s++,0);rt_key(13,0);}
static void original(void) {
    char path[16];uint8_t r;uint16_t i,before;
    strcpy(path,"games/sq1");r=rt_init(path);if(!r)r=scan();
    if(!r){rt_key(' ',0);r=scan();}
    if(r==VM_WAIT&&rt.ui==RT_UI_STRING){sentence("Roger");r=scan();}
    for(i=0;i<100&&!r&&!rt.player_control;++i)r=scan();
    check(!r&&rt_vm.vars[0]==2&&rt.player_control,"unmodified SQ1 reaches playable opening room");if(r)return;
    /* Menu controller 10 lists inventory; 22 invokes the original select /
       show.obj mapping. Staged possessions are explicit integration fixtures. */
    rt.pending[10>>3]|=1u<<(10&7);r=scan();
    check(r==VM_WAIT&&rt.ui==RT_UI_INVENTORY,"original SQ1 inventory menu opens status");
    rt_key(13,0);r=scan();check(!r,"original inventory list resumes game scan");if(r)return;
    memset(rt.locations,0,sizeof(rt.locations));command(0x5c,5);
    rt.pending[22>>3]|=1u<<(22&7);r=scan();
    check(r==VM_WAIT&&rt.ui==RT_UI_INVENTORY&&agi_vm_flag(&rt_vm,13),"original examine-object menu requests selectable inventory");
    rt_key(13,0);r=scan();check(r==VM_WAIT&&rt.ui==RT_UI_OBJECT&&rt_vm.vars[25]==5,"original script maps selected keycard to VIEW 219");
    screen("build/inventory-sq1-keycard.l2");rt_key(13,0);r=scan();
    check(!r&&!agi_vm_flag(&rt_vm,13)&&rt.locations[5]==255,"original inspection returns to game and retains possession");if(r)return;
    memset(rt.locations,0,sizeof(rt.locations));command(0x5c,11);
    rt.pending[22>>3]|=1u<<(22&7);r=scan();rt_key(13,0);r=scan();
    check(r==VM_WAIT&&rt.ui==RT_UI_OBJECT&&rt.locations[11]==255&&!rt.locations[12],"survival kit contents remain unchanged while description is open");
    screen("build/inventory-sq1-kit.l2");rt_key(13,0);r=scan();
    check(!r&&!rt.locations[11]&&rt.locations[12]==255&&rt.locations[19]==255&&rt.locations[22]==255,"original survival-kit script grants water, knife and opened kit after dismissal");
    /* The buckazoid description includes a live variable in the VIEW text. */
    rt_vm.vars[124]=37;before=checksum(40,10);r=command(0x81,224);
    screen("build/inventory-sq1-buckazoids.l2");rt_key(13,0);
    check(r==VM_WAIT&&!rt.ui&&checksum(40,10)==before,"original buckazoid VIEW expands and restores without script patching");
}
uint8_t bank_inventory_game_tests(void) {
    char path[40];strcpy(path,"build/inventory-game-tests.txt");file=esx_f_open(path,ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC);
    checks=failures=0;original();
    sprintf(rt_buffer,"SUMMARY checks=%u failures=%u\n",(unsigned)checks,(unsigned)failures);esx_f_write(file,rt_buffer,strlen(rt_buffer));esx_f_close(file);return !failures;
}
