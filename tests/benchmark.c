/* Emulated 50 Hz frame timing; CSpect host file latency is not included. */
#include <arch/zxn.h>
#include <arch/zxn/esxdos.h>
#include <errno.h>
#include <im2.h>
#include <intrinsic.h>
#include <stdio.h>
#include <string.h>
#include "agi/picture.h"
#include "next/platform.h"
#include "next/resource.h"
static volatile uint16_t frames;
static uint8_t resource[4096];
static const uint8_t pictures[] = {1,12,27,67};
static char report[100];
IM2_DEFINE_ISR(frame_interrupt) { ++frames; }
static uint16_t now(void) {
    uint16_t f;
    intrinsic_di(); f=frames; intrinsic_ei(); return f;
}
int main(void) {
    uint8_t i, h, result;
    uint16_t length, start, clear_frames, decode_frames, show_frames;
    next_init();
    memset((void *)0xf000,0xf1,257);
    *((uint8_t *)0xf1f1)=0xc3;
    *((void **)(0xf1f2))=frame_interrupt;
    im2_init((void *)0xf000); intrinsic_ei();
    errno=0; h=esx_f_open("build/benchmark.txt",ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC);
    if(errno) { next_text(0,3,"BENCHMARK LOG OPEN FAILED"); for(;;){} }
    for(i=0;i<sizeof(pictures);++i) {
        result=resource_load("games/sq1","PICDIR",pictures[i],resource,sizeof(resource),&length);
        if(result) break;
        start=now(); agi_picture_clear(); clear_frames=now()-start;
        start=now(); result=agi_picture_decode(resource,length); decode_frames=now()-start;
        start=now(); next_show_picture(0); show_frames=now()-start;
        sprintf(report,"PIC %u clear=%u decode=%u show=%u result=%u frames\n",(unsigned int)pictures[i],clear_frames,decode_frames,show_frames,(unsigned int)result);
        esx_f_write(h,report,strlen(report));
        next_text(0,3,report);
    }
    esx_f_close(h); intrinsic_di(); next_text(0,3,"BENCHMARK COMPLETE                    ");
    for(;;){}
}
