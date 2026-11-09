#include <arch/zxn/esxdos.h>
#include <arch/zxn.h>
#include <errno.h>
#include <string.h>
#include "runtime.h"
#include "platform.h"
uint8_t rt_startup(void) {
    char path[101];uint8_t f;uint16_t n;
    /* Real Next cores with extended-key registers: read dedicated keys once,
       suppress their synthesized matrix chords. Retain emulator fallback. */
    if(ZXN_READ_REG(0x00)==10 && (ZXN_READ_REG(0x01)>0x31 ||
       (ZXN_READ_REG(0x01)==0x31 && ZXN_READ_REG(0x0e)>=5)))
        ZXN_WRITE_REG(0x68,ZXN_READ_REG(0x68)|16);
    strcpy(path,"ZXQUEST.CFG");errno=0;
    f=esx_f_open(path,ESX_MODE_READ|ESX_MODE_OPEN_EXIST);
    if(errno==ESX_ENOENT)strcpy(path,"games/sq1");
    else {
        if(errno)return VM_IO;
        n=esx_f_read(f,path,100);esx_f_close(f);
        if(errno || !n || n==100)return VM_ARGUMENT;
        path[n]=0;while(n && (path[n-1]=='\n' || path[n-1]=='\r'))path[--n]=0;
        if(!n)return VM_ARGUMENT;
    }
    strcpy(rt_buffer,"LOADING ORIGINAL GAME FILES");next_text(0,5,rt_buffer);
    return rt_init(path);
}
