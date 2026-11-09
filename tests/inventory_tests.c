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
static uint8_t objects(const char *directory) {
    char path[40];strcpy(path,directory);return rt_objects(path);
}
static uint8_t init(void) {
    char path[40];strcpy(path,"build/inventory-fixtures");return rt_init(path);
}
static uint8_t script(uint8_t id) {
    uint8_t r=agi_vm_start(&rt_vm,id);uint16_t n=0;
    while(r==VM_RUNNING && n++<500)r=agi_vm_run(&rt_vm,100);
    return r;
}
static uint8_t resume(void) {
    uint8_t r=agi_vm_resume(&rt_vm);uint16_t n=0;
    while(r==VM_RUNNING && n++<500)r=agi_vm_run(&rt_vm,100);
    return r;
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
static void seed(void) {
    uint8_t page,saved=ZXN_READ_REG(0x53),y;uint16_t at;
    for(page=40;page<50;++page){ZXN_WRITE_REG(0x53,page);for(at=0;at<8192;++at){y=at;*((uint8_t *)(0x6000u+at))=y<32 || y>=232?0xa7:(at+page)&15;}}
    ZXN_WRITE_REG(0x53,saved);
}
static void text(uint8_t row,uint8_t col,const char *s) {strcpy(rt_buffer,s);rt_text_at(row,col,rt_buffer,0);}
static uint8_t pixel(uint16_t x,uint8_t y,uint8_t colour,uint8_t write) {
    uint8_t value,saved=ZXN_READ_REG(0x53);uint8_t *p;
    ZXN_WRITE_REG(0x53,40+(x>>5));p=(uint8_t *)(0x6000u+((x&31)<<8)+y);
    if(write)*p=colour;value=*p;ZXN_WRITE_REG(0x53,saved);return value;
}
static void names(void) {
    char name[41];uint8_t r,saved=ZXN_READ_REG(0x53);
    check(!init()&&rt.objects==43&&rt.locations[1]==255&&rt.locations[2]==0,"encrypted OBJECT loads names and original locations");
    check(!rt_object_name(3,name)&&!strcmp(name,"Survival Kit"),"name offset is relative to byte 3 and XOR phase is absolute");
    check(!rt_object_name(4,name)&&strlen(name)==40,"long name clips safely at forty characters");
    check(rt_object_name(43,name)==VM_ARGUMENT,"invalid object name index is rejected");
    check(!objects("build/inventory-plain")&&!rt_object_name(1,name)&&!strcmp(name,"Keycard"),"unencrypted OBJECT names are supported");
    check(objects("build/inventory-bad-offset")==VM_HEADER&&!rt.objects,"offset into object table is rejected");
    check(objects("build/inventory-short")==VM_HEADER&&!rt.objects,"truncated object table is rejected");
    r=objects("build/inventory-bad-name");seed();
    check(!r&&rt_object_name(0,name)==VM_HEADER,"unterminated object name is rejected");
    check(command(0x7c,0)==VM_HEADER&&!rt.ui,"bad carried name fails before displaying inventory");
    check(!objects("build/inventory-empty")&&!rt.objects,"empty encrypted OBJECT is accepted");
    check(ZXN_READ_REG(0x53)==saved,"name reads and loader errors preserve MMU3");
    init();rt_vm.vars[3]=42;
    check(!rt_format(4,1,rt_buffer,sizeof(rt_buffer))&&!strcmp(rt_buffer,"Name: Survival Kit. Score: 42."),"LOGIC messages expand object names with percent-n");
    check(rt_format(4,2,rt_buffer,sizeof(rt_buffer))==VM_ARGUMENT&&rt_format(4,3,rt_buffer,sizeof(rt_buffer))==VM_ARGUMENT,"object substitutions reject missing and overflowing IDs");
}
static void inventory(void) {
    uint8_t r,i;uint16_t before,start;resource_cache_info info;
    init();seed();before=checksum(40,10);rt.fg=14;rt.bg=3;rt_vm.vars[25]=123;
    r=script(1);resource_cache_status(&info);
    check(r==VM_WAIT&&rt.ui==RT_UI_INVENTORY&&!rt_vm.vars[205]&&info.pinned==1,"status suspends VM and retains executing LOGIC pin");
    check(rt.fg==14&&rt.bg==3&&rt_step(250)==VM_WAIT,"inventory preserves text colours and pauses game cycles");
    rt_key('x',0);resource_cache_status(&info);
    check(!rt.ui&&rt_vm.vars[25]==123&&!rt.key_pending&&!rt_vm.vars[19]&&checksum(40,10)==before,"ordinary status closes on a key without changing selection or leaking input");
    check(!resume()&&rt_vm.vars[205]==77,"status resumes at the instruction after its wait");
    resource_cache_status(&info);check(!info.pinned,"completed inventory scan releases LOGIC pin");
    agi_vm_set_flag(&rt_vm,13,1);r=command(0x7c,0);screen("build/inventory-list.l2");
    rt_key(0,0x48);rt_key(0,0x4b);rt_key(13,0);
    check(r==VM_WAIT&&rt_vm.vars[25]==1,"selection starts at first carried ID and bounds up/left");
    command(0x7c,0);rt_key(0,0x4d);rt_key(13,0);check(rt_vm.vars[25]==3,"right selects next carried item rather than adjacent OBJECT index");
    command(0x7c,0);rt_key(0,0x50);rt_key(13,0);check(rt_vm.vars[25]==4,"down moves to item in next row");
    command(0x7c,0);rt_key(0,0x50);rt_key(0,0x4d);rt_key(0,0x50);rt_key(0,0x4d);rt_key(13,0);
    check(rt_vm.vars[25]==42,"last item bounds down/right");
    command(0x7c,0);rt_key('z',0);check(rt.ui==RT_UI_INVENTORY,"typing does not dismiss selectable inventory");
    start=next_clock();rt_key(27,0);r=next_clock()-start;
    check(rt_vm.vars[25]==255&&checksum(40,10)==before,"cancel returns 255 and restores every display byte including margins");
    sprintf(rt_buffer,"Full inventory restore=%u emulated frames\n",(unsigned)r);esx_f_write(file,rt_buffer,strlen(rt_buffer));
    memset(rt.locations,0,sizeof(rt.locations));command(0x7c,0);rt_key(13,0);
    check(rt_vm.vars[25]==255&&!rt.ui,"empty selection safely returns 255");
    objects("build/inventory-max");command(0x7c,0);
    for(i=0;i<20;++i)rt_key(0,0x50);rt_key(13,0);
    check(rt_vm.vars[25]==40,"selection crosses forty-item page boundary");
    command(0x7c,0);for(i=0;i<127;++i)rt_key(0,0x50);rt_key(13,0);
    check(rt.objects==256&&rt_vm.vars[25]==254,"256-object table does not wrap count or navigation");
    agi_vm_set_flag(&rt_vm,13,0);command(0x7c,0);rt_key(0,0x50);
    check(rt.ui==RT_UI_INVENTORY,"ordinary large inventory can page down");
    rt_key(0,0x48);rt_key('x',0);check(!rt.ui&&rt_vm.vars[25]==254&&checksum(40,10)==before,"ordinary paging preserves v25 and background");
}
static void inspection(void) {
    uint8_t r,i,saved2=ZXN_READ_REG(0x52),saved3=ZXN_READ_REG(0x53),saved4=ZXN_READ_REG(0x54);
    uint16_t before,actual,scene,actors;resource_cache_info info;
    init();seed();before=checksum(40,10);scene=checksum(32,8);actors=checksum(60,1);
    rt_vm.vars[3]=42;strcpy(rt.strings[1],"Roger");r=script(2);actual=checksum(40,10);resource_cache_status(&info);
    check(r==VM_WAIT&&rt.ui==RT_UI_OBJECT&&info.pinned==1,"show.obj suspends script but releases VIEW after drawing");
    check(pixel(156,214,0,0)==4&&pixel(158,214,0,0)==3&&pixel(160,214,0,0)==2&&pixel(156,215,0,0)==15&&pixel(160,215,0,0)==5,"inspection respects mirror, transparent colour and doubled AGI pixels");
    screen("build/inventory-inspection.l2");rt_modal(RT_MODAL_CLEAR);rt.fg=0;rt.bg=15;
    text(2,2,"A keycard with 42 credits. Survival");text(3,2,"Kit. Nested Roger. Tail.");text(24,2,"ENTER or Shift+1: return to game");
    for(i=0;i<6;++i)pixel(156+i,214,4-i/2,1);pixel(160,215,5,1);pixel(161,215,5,1);
    check(actual==checksum(40,10),"inspection word wrap and nested substitutions match hand-laid-out frame");
    rt_key('x',0);check(rt.ui==RT_UI_OBJECT,"unrelated key leaves inspection open");rt_key(13,0);
    check(!rt.ui&&checksum(40,10)==before&&checksum(32,8)==scene&&checksum(60,1)==actors,"inspection restores framebuffer without changing scene, priority or actors");
    check(!resume()&&rt_vm.vars[205]==88,"show.obj continues original script after dismissal");
    rt_vm.vars[206]=1;r=script(3);rt_key(27,0);
    check(r==VM_WAIT&&!resume()&&rt_vm.vars[205]==99&&checksum(40,10)==before,"show.obj.v reads VIEW ID from variable and resumes on cancel");
    for(i=2;i<=4;++i)check(command(0x81,i)==VM_HEADER&&!rt.ui&&checksum(40,10)==before,"malformed VIEW fails atomically without a modal or pixel damage");
    check(command(0x81,8)==VM_HEADER&&!rt.ui,"overlong raw description is rejected within fixed bound");
    r=command(0x81,9);rt_key(27,0);check(r==VM_WAIT&&!rt.ui&&checksum(40,10)==before,"511-character description fits exactly and cancels without pixel damage");
    check(command(0x81,250)==VM_IO&&!rt.ui,"missing VIEW returns explicit IO failure");
    r=command(0x81,5);rt_key(13,0);check(r==VM_WAIT&&!rt.ui&&checksum(40,10)==before,"description-free VIEW can be inspected and dismissed");
    r=command(0x81,6);rt_key(13,0);check(r==VM_WAIT&&!rt.ui&&checksum(40,10)==before,"oversized cel clips to image panel and preserves margins");
    r=command(0x81,7);rt_key(13,0);rt_key(13,0);check(r==VM_WAIT&&rt.ui==RT_UI_OBJECT,"long descriptions continue across pages without lost lines");
    screen("build/inventory-last-page.l2");actual=checksum(40,10);rt_modal(RT_MODAL_CLEAR);rt.fg=0;rt.bg=15;
    text(2,2,"Page line 15");text(3,2,"Page line 16");text(24,2,"ENTER or Shift+1: return to game");
    for(i=0;i<6;++i)pixel(156+i,214,4-i/2,1);pixel(160,215,5,1);pixel(161,215,5,1);
    check(actual==checksum(40,10),"final description page has exactly the remaining authored lines");
    rt_key(13,0);check(!rt.ui&&checksum(40,10)==before,"last description page closes and restores original display");
    resource_cache_status(&info);check(!info.pinned,"successful and malformed inspections leak no resource pins");
    check(ZXN_READ_REG(0x52)==saved2&&ZXN_READ_REG(0x53)==saved3&&ZXN_READ_REG(0x54)==saved4,"nested inventory, text and VIEW calls preserve all bank mappings");
}
uint8_t bank_inventory_tests(void) {
    char path[40];uint8_t i,ok=1;
    strcpy(path,"build/inventory-tests.txt");file=esx_f_open(path,ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC);
    memset((void *)0xfb00,0xa5,64);checks=failures=0;
    names();inventory();inspection();
    for(i=0;i<64;++i)if(*((uint8_t *)(0xfb00u+i))!=0xa5)ok=0;
    check(ok,"inventory call chains preserve stack guard");
    sprintf(rt_buffer,"SUMMARY checks=%u failures=%u\n",(unsigned)checks,(unsigned)failures);esx_f_write(file,rt_buffer,strlen(rt_buffer));esx_f_close(file);
    return !failures;
}
