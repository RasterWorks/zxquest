#include <arch/zxn.h>
#include <arch/zxn/esxdos.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "agi/picture.h"
#include "next/platform.h"
#include "next/resource_cache.h"
#include "../build/resource_manifest.h"

static uint8_t buffer[512];
static char message[100];
static uint16_t failures, checks, loaded;
static uint8_t log_file;
static resource_cache_info info;
static resource_handle handles[32];
static const uint16_t crc_table[16] = {
    0,0x1021,0x2042,0x3063,0x4084,0x50a5,0x60c6,0x70e7,
    0x8108,0x9129,0xa14a,0xb16b,0xc18c,0xd1ad,0xe1ce,0xf1ef
};
static void log_line(const char *line) {
    if (esx_f_write(log_file, (void *)line, strlen(line)) != strlen(line)) {
        next_text(0, 28, "RESOURCE TEST LOG FAILED"); for (;;) {}
    }
}
static void check(uint8_t ok, const char *name) {
    ++checks; if (!ok) ++failures;
    sprintf(message, "%s %s\n", ok ? "PASS" : "FAIL", name); log_line(message);
}
static uint16_t crc16(uint16_t crc, uint16_t count) {
    uint16_t i;
    for (i=0; i<count; ++i) {
        crc = (crc << 4) ^ crc_table[(crc >> 12) ^ (buffer[i] >> 4)];
        crc = (crc << 4) ^ crc_table[(crc >> 12) ^ (buffer[i] & 15)];
    }
    return crc;
}
static uint8_t pattern(resource_handle handle, uint8_t seed, uint16_t length) {
    uint16_t at=0, count, i;
    while (at < length) {
        count = length - at; if (count > sizeof(buffer)) count = sizeof(buffer);
        if (resource_copy(handle, at, buffer, count)) return 0;
        for (i=0; i<count; ++i)
            if (buffer[i] != (uint8_t)((at+i)*13u + ((at+i)>>8) + seed*17u)) return 0;
        at += count;
    }
    return 1;
}
static void fixture_tests(void) {
    resource_handle h=0, again=0, old;
    uint16_t size, i;
    uint8_t result, ok, saved2, saved3;
    resource_cache_init("build/resource-fixtures");
    check(resource_acquire(99, 1, &h) == RES_ARGUMENT && !h, "invalid resource type");
    check(resource_acquire(0, 7, &h) == RES_MISSING && !h, "absent directory entry");
    check(resource_acquire(0, 5, &h) == RES_HEADER && !h, "bad first signature byte");
    check(resource_acquire(0, 8, &h) == RES_HEADER && !h, "bad second signature byte");
    check(resource_acquire(0, 9, &h) == RES_HEADER && !h, "wrong volume in header");
    check(resource_acquire(0, 10, &h) == RES_IO && !h, "truncated volume header");
    for (i=0; i<40; ++i) {
        result = resource_acquire(0, 6, &h);
        if (result != RES_IO || h) break;
    }
    resource_cache_status(&info);
    check(i==40 && !info.pages_used && !info.entries, "failed loads leave no cached entries or pages");
    check(!resource_acquire(0, 0, &h) && !resource_size(h, &size) && !size &&
          !resource_copy(h, 0, buffer, 0) && resource_copy(h, 0, buffer, 1)==RES_RANGE, "empty resource bounds");
    resource_release(h);
    check(!resource_acquire(0, 1, &h) && pattern(h,1,8191), "8191-byte payload");
    resource_release(h);
    check(!resource_acquire(0, 2, &h) && pattern(h,2,8192), "8192-byte payload");
    resource_release(h);
    check(!resource_acquire(0, 3, &h) && pattern(h,3,8193), "8193-byte payload across bank");
    check(!resource_copy(h,8190,buffer,3) && buffer[0]==56 && buffer[1]==69 && buffer[2]==83,
          "single copy crosses 8 KB boundary");
    resource_release(h);
    result=resource_acquire(RESOURCE_LOGIC,2,&h);
    ok=!resource_acquire(RESOURCE_PICTURE,2,&again);
    check(!result && ok && h!=again && pattern(again,2,8192), "resource type is part of cache identity");
    if(!result) resource_release(h);
    if(ok) resource_release(again);
    check(resource_load("build/resource-fixtures","LOGDIR",2,buffer,sizeof(buffer),&size)==RES_TOO_LARGE && size==8192,
          "legacy flat loader retains capacity check");
    agi_pixel_set(1234,0xa7);
    saved2 = ZXN_READ_REG(0x52);
    ZXN_WRITE_REG(0x53,54); memset((void *)0x6000,0x9c,8192);
    saved3 = ZXN_READ_REG(0x53);
    check(!resource_acquire(0,4,&h) && !resource_size(h,&size) && size==65535u && pattern(h,4,size), "65535-byte payload across eight banks");
    check(ZXN_READ_REG(0x52)==saved2 && ZXN_READ_REG(0x53)==saved3 && agi_pixel_get(1234)==0xa7,
          "banked IO and copies preserve MMU2 MMU3 and picture");
    for (i=0; i<8192 && ((uint8_t *)0x6000)[i]==0x9c; ++i) {}
    check(i==8192, "display-window canary survives resource IO");
    check(!resource_copy(h,65534u,buffer,1) && !resource_copy(h,65535u,buffer,0) &&
          resource_copy(h,65534u,buffer,2)==RES_RANGE && resource_copy(h,65500u,buffer,100)==RES_RANGE,
          "end-of-resource and 16-bit overflow bounds");
    check(!resource_acquire(0,4,&again) && again==h, "cache hit returns same handle");
    resource_release(h); resource_release(again);
    check(resource_release(h)==RES_ARGUMENT, "unbalanced release rejected");
    old = h;
    resource_cache_init("build/resource-fixtures");
    check(resource_size(old,&size)==RES_STALE && resource_copy(0,0,buffer,1)==RES_STALE, "reset invalidates handles");

    /* Fill the entire pool with pinned entries. A failed request cannot evict them. */
    ok = 1;
    for (i=0; i<RESOURCE_CACHE_PAGES; ++i) if (resource_acquire(0,16+i,&handles[i])) ok = 0;
    check(ok, "configured cache pool fills with pinned resources");
    check(resource_acquire(0,4,&h)==RES_NO_MEMORY && !h, "full pinned cache reports capacity");
    check(!resource_copy(handles[0],0,buffer,1) && buffer[0]==(uint8_t)(16*17) &&
          !resource_size(handles[RESOURCE_CACHE_PAGES-1],&size) && size==8192, "capacity failure preserves pinned resources");
    for (i=0; i<RESOURCE_CACHE_PAGES; i+=2) resource_release(handles[i]);
    check(!resource_acquire(0,4,&h) && pattern(h,4,65535u), "large load uses fragmented free pages");
    check(resource_size(handles[2],&size)==RES_STALE && !resource_size(handles[1],&size), "evicted handles stale and pinned handles valid");
    resource_cache_status(&info);
    check(info.pages_used==RESOURCE_CACHE_PAGES && info.evictions==8 && info.pinned==RESOURCE_CACHE_PAGES/2+1, "fragmented eviction accounting");
    /* Init clears the cache even when old resources are pinned. */
    old = h; resource_cache_init("games/sq1");
    check(resource_size(old,&size)==RES_STALE, "game-directory switch invalidates old handle");
    resource_cache_init("build/resource-fixtures");
    resource_acquire(0,1,&h);
    for (i=1; i<255; ++i) if (resource_acquire(0,1,&again)) break;
    check(i==255 && resource_acquire(0,1,&again)==RES_PIN_LIMIT && !again, "pin count cannot wrap");
    for (i=0; i<255; ++i) resource_release(h);
    resource_cache_status(&info);
    check(!info.pinned && info.hits==254, "all pins released after repeated acquire");

    /* An access updates LRU order; keep resource 16 and evict 17. */
    resource_cache_init("build/resource-fixtures");
    for (i=0; i<RESOURCE_CACHE_PAGES; ++i) { resource_acquire(0,16+i,&handles[i]); resource_release(handles[i]); }
    resource_copy(handles[0],0,buffer,1);
    result = resource_acquire(0,48,&h);
    check(!result && !resource_size(handles[0],&size) && resource_size(handles[1],&size)==RES_STALE,
          "least recently used unpinned resource evicted");
}
static void game_tests(void) {
    uint16_t n, size, at, count, crc;
    uint8_t result, saved2, saved3;
    resource_handle h, again;
    const struct expected_resource *e;
    resource_cache_init("games/sq1");
    for (n=0; n<sizeof(expected)/sizeof(expected[0]); ++n) {
        e = &expected[n];
        saved2=ZXN_READ_REG(0x52); saved3=ZXN_READ_REG(0x53);
        result = resource_acquire(e->type,e->number,&h);
        if (result != e->result || (result && h)) {
            ++failures; sprintf(message,"FAIL resource %u:%u result=%u expected=%u\n",(unsigned)e->type,(unsigned)e->number,(unsigned)result,(unsigned)e->result); log_line(message);
            continue;
        }
        if (result) { check(result==RES_IO,"known SNDDIR 0 missing VOL.15 reported"); continue; }
        crc=0xffff; at=0;
        resource_size(h,&size);
        while (at < size) {
            count=size-at; if(count>sizeof(buffer)) count=sizeof(buffer);
            result=resource_copy(h,at,buffer,count);
            if(result) break;
            crc=crc16(crc,count); at+=count;
        }
        if(result || size!=e->length || crc!=e->crc || ZXN_READ_REG(0x52)!=saved2 || ZXN_READ_REG(0x53)!=saved3) {
            ++failures; sprintf(message,"FAIL resource %u:%u size=%u crc=%04x expected=%04x\n",(unsigned)e->type,(unsigned)e->number,size,crc,e->crc); log_line(message);
        } else ++loaded;
        result=resource_acquire(e->type,e->number,&again);
        if(result || again!=h) ++failures;
        if(!result) resource_release(again);
        resource_release(h);
        if(!(n&31)) { sprintf(message,"RESOURCES CHECKED %u            ",n+1); next_text(0,5,message); }
    }
    resource_cache_status(&info);
    check(!info.pinned && info.hits==loaded && info.evictions>0, "real-game cache hits release and eviction");
}
int main(void) {
    next_init(); next_text(0,3,"ZXQUEST BANKED RESOURCE TESTS");
    errno=0; log_file=esx_f_open("build/resource-tests.txt",ESX_MODE_WRITE | ESX_MODE_OPEN_CREAT_TRUNC);
    if(errno) { next_text(0,5,"CANNOT OPEN RESOURCE TEST LOG"); for (;;) {} }
    fixture_tests();
    game_tests();
    sprintf(message,"SUMMARY resources=%u checks=%u failures=%u\n",loaded,checks,failures);
    log_line(message); esx_f_close(log_file);
    sprintf(message,"RESOURCES VERIFIED %u           ",loaded); next_text(0,5,message);
    next_text(0,7,failures ? "RESOURCE TESTS FAILED" : "RESOURCE TESTS PASSED");
    for (;;) {}
}
