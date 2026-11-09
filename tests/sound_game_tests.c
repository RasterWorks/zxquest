#include <arch/zxn/esxdos.h>
#include <string.h>
#include <stdio.h>
#include "next/runtime.h"
#include "next/sound.h"
#include "../build/runtime_manifest.h"
static char path[40];
static uint8_t file;
static void line(void) { esx_f_write(file,rt_buffer,strlen(rt_buffer)); }
uint8_t bank_sound_game_tests(void) {
    uint8_t n,r,ok=1;uint16_t i,frames,start,base=0,active=0,total=0,errors=0;
    strcpy(path,"build/sound-game-tests.txt");file=esx_f_open(path,ESX_MODE_WRITE|ESX_MODE_OPEN_CREAT_TRUNC);
    strcpy(path,"games/sq1");next_sound_manual=1;r=rt_init(path);
    if(r){sprintf(rt_buffer,"FAIL initialization %u\n",(unsigned)r);line();esx_f_close(file);return 0;}
    for(n=0;n<50;++n) {
        r=rt_sound_start(expected_sounds[n].number,200);
        frames=(expected_sounds[n].ticks*5+5)/6;
        if(r||rt.sound_ticks!=expected_sounds[n].ticks){++errors;continue;}
        start=next_clock();for(i=0;i<frames;++i){}base+=next_clock()-start;
        start=next_clock();for(i=0;i<frames;++i)next_sound_frame();active+=next_clock()-start;
        total+=frames;rt_sound_poll();
        if(sound_playing||!agi_vm_flag(&rt_vm,200)||sound_ticks!=expected_sounds[n].ticks)++errors;
    }
    sprintf(rt_buffer,"BENCH all 50 original sounds: scheduled frames=%u clock-only=%u player=%u additional=%u\n",total,base,active,active-base);line();
    sprintf(rt_buffer,"%s all 50 sound endings match independent host durations, completion flags and no resource errors\n",errors?"FAIL":"PASS");line();
    /* Include one full frame of measurement uncertainty per sound as a
       conservative upper bound on accumulated timer quantization. */
    if((uint32_t)(active-base+50)*100>=(uint32_t)total*2)ok=0;
    sprintf(rt_buffer,"%s average whole-player CPU below 2 percent, including 50-frame quantization margin\n",ok?"PASS":"FAIL");line();
    sprintf(rt_buffer,"SUMMARY checks=2 failures=%u\n",(unsigned)((errors!=0)+!ok));line();
    rt_sound_stop();esx_f_close(file);return ok&&!errors;
}
