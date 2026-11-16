/* Save placement follows the resource folder, independent of launcher mode.
   Legacy fingerprint folders are read-only fallbacks for an absent flat slot. */
#include <arch/zxn/esxdos.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "save.h"
#include "resource_cache_internal.h"
#ifdef SAVE_PATH_TEST
/* Exercise real-Next missing-parent errors with CSpect's host filesystem. */
uint8_t rt_save_probe_error;
#endif

uint8_t rt_save_path(uint8_t slot,uint8_t restore,uint32_t identity,char *path) {
    uint8_t mode,copy,f;uint16_t end;
    /* cache_game_directory is at most 100 chars; caller provides 128 bytes. */
    for(mode=0;mode<3;++mode) {
        if(!mode)sprintf(path,"%s/saves/%u.a",cache_game_directory,(unsigned)slot);
        else sprintf(path,"%s/saves/%04X%04X/%u.a",mode==1?(const char *)cache_game_directory:".",
            (unsigned)(identity>>16),(unsigned)identity,(unsigned)slot);
        if(!restore)return SAVE_OK;
        end=strlen(path)-1;
        for(copy=0;copy<2;++copy) {
            path[end]='a'+copy;errno=0;f=esx_f_open(path,ESX_MODE_READ|ESX_MODE_OPEN_EXIST);
            if(!errno){esx_f_close(f);path[end]='a';return SAVE_OK;}
#ifdef SAVE_PATH_TEST
            if(rt_save_probe_error)errno=rt_save_probe_error;
#endif
            /* A missing intermediate directory can be EPATH on NextZXOS.
               This is normal after removing the old fingerprint folders;
               keep genuine access/device/IO failures visible to the user. */
            if(errno!=ESX_ENOENT && errno!=ESX_EPATH)return SAVE_IO;
        }
    }
    return SAVE_EMPTY;
}
