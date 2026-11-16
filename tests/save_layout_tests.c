#include <arch/zxn/esxdos.h>
#include <arch/zxn.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "next/runtime.h"
#include "next/save.h"
#include "../build/save_manifest.h"
extern uint8_t rt_save_description(uint8_t,char *);
extern char rt_save_label[32];
extern uint8_t rt_save_probe_error;
static uint8_t file,checks,failures;
static void check(uint8_t ok,const char *name){
    ++checks;if(!ok)++failures;sprintf(rt_buffer,"%s %s\n",ok?"PASS":"FAIL",name);
    esx_f_write(file,rt_buffer,strlen(rt_buffer));
}
static uint8_t copy(const char *destination){
    char path[128];uint8_t in,out,ok=1;uint16_t n;
    strcpy(path,SAVE_ROOT "/1.a");errno=0;in=esx_f_open(path,ESX_MODE_READ|ESX_MODE_OPEN_EXIST);if(errno)return 0;
    strcpy(path,destination);out=esx_f_open(path,ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC);
    if(errno){esx_f_close(in);return 0;}
    while((n=esx_f_read(in,rt_buffer,512))!=0)if(esx_f_write(out,rt_buffer,n)!=n){ok=0;break;}
    if(errno)ok=0;esx_f_close(in);esx_f_close(out);return ok;
}
uint8_t bank_save_layout_tests(void){
    char path[128],label[32],expected[32];uint8_t r,f,i,blank,saved;
    strcpy(path,"build/save-layout-tests.txt");file=esx_f_open(path,ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC);checks=failures=0;
    strcpy(path,"build/runtime-fixtures");check(!rt_init(path),"layout fixture initialized");
    check(!rt_save_description(1,expected),"new saves and labels read directly from game/saves");
    check(copy(SAVE_LEGACY_LOCAL "/8.a"),"prepared game-local legacy slot");
    check(!rt_save_description(8,label) && !strcmp(label,expected),"legacy game-local description found");
    check(!rt_save_slot(8,1) && rt_vm.vars[200]==123,"legacy game-local snapshot restores");
    check(copy(SAVE_LEGACY_ROOT "/9.b"),"prepared old launch-root legacy slot with only b copy");
    check(!rt_save_description(9,label) && !strcmp(label,expected),"legacy launch-root description found");
    check(!rt_save_slot(9,1) && rt_vm.vars[200]==123,"legacy launch-root snapshot restores");
    rt_vm.vars[200]=88;strcpy(rt_save_label,"Flat save");r=rt_save_slot(8,0);rt_save_label[0]=0;
    check(!r && !rt_save_slot(8,1) && rt_vm.vars[200]==88,"new flat save takes precedence over legacy slot");
    check(!rt_save_description(8,label) && !strcmp(label,"Flat save"),"flat description takes precedence too");
    strcpy(path,SAVE_ROOT "/8.a");f=esx_f_open(path,ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC);esx_f_write(f,"short",5);esx_f_close(f);
    check(rt_save_slot(8,1)==SAVE_INVALID && rt_vm.vars[200]==88,"corrupt flat slot does not silently restore stale legacy data");
    check(rt_save_slot(7,0)==SAVE_GAME,"saving refuses to overwrite another game's flat slot");
    check(rt_save_description(7,label)==SAVE_GAME,"foreign save remains intact after refused overwrite");
    strcpy(path,"build/runtime-fixtures");esx_f_chdir(path);strcpy(path,".");r=rt_init(path);
    check(!r && !rt_save_slot(1,1) && rt_vm.vars[200]==123,"direct launch reads same saves as explicit CFG launch");
    rt_vm.vars[200]=44;check(!rt_save_slot(4,0),"direct launch writes flat saves");
    strcpy(path,"/");esx_f_chdir(path);strcpy(path,"build/runtime-fixtures");r=rt_init(path);
    check(!r && !rt_save_slot(4,1) && rt_vm.vars[200]==44,"explicit launch restores direct-launch save");
    strcpy(path,"build/empty-save-fixtures");check(!rt_init(path),"game without any save folders initializes");
    check(rt_save_description(1,label)==SAVE_EMPTY,"missing save directories leave unused slots empty");
    rt_save_probe_error=ESX_EPATH;
    check(rt_save_description(1,label)==SAVE_EMPTY,"NextZXOS missing-parent EPATH is an empty slot");
    rt_save_ui(0x7d,0,0);saved=ZXN_READ_REG(0x53);ZXN_WRITE_REG(0x53,61);blank=1;
    for(i=0;i<9;++i)if(*((uint8_t *)(0x7400u+(uint16_t)i*32)))blank=0;
    ZXN_WRITE_REG(0x53,saved);rt_save_ui(0,27,0);
    check(blank,"save dialog leaves all nine missing slots blank");
    rt_save_ui(0x7e,0,0);saved=ZXN_READ_REG(0x53);ZXN_WRITE_REG(0x53,61);blank=1;
    for(i=0;i<9;++i)if(*((uint8_t *)(0x7400u+(uint16_t)i*32)))blank=0;
    ZXN_WRITE_REG(0x53,saved);rt_save_ui(0,27,0);
    check(blank,"restore dialog leaves all nine missing slots blank");
    rt_save_probe_error=ESX_EIO;check(rt_save_description(1,label)==SAVE_IO,"real IO failure is not hidden as empty");
    rt_save_probe_error=ESX_EACCES;check(rt_save_description(1,label)==SAVE_IO,"access denial is not hidden as empty");
    rt_save_probe_error=ESX_ENOTDIR;check(rt_save_description(1,label)==SAVE_IO,"file used as a folder is not hidden as empty");
    rt_save_probe_error=0;
    sprintf(rt_buffer,"SUMMARY checks=%u failures=%u\n",(unsigned)checks,(unsigned)failures);
    esx_f_write(file,rt_buffer,strlen(rt_buffer));esx_f_close(file);return !failures;
}
