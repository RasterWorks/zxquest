#include <arch/zxn/esxdos.h>
#include <arch/zxn.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "agi/picture.h"
#include "next/platform.h"
#include "next/resource.h"

static uint8_t data[4096];
static char message[100];
static uint8_t log_handle;
static unsigned int failures;
uint8_t agi_picture_decode_reference(const uint8_t *data, uint16_t length);

/* Pages 52..55 retain the original decoder output during regression tests. */
static void save_reference(void) {
    uint16_t at;
    for (at=0; at<AGI_PIXELS; ++at) {
        if (!(at & 8191)) ZXN_WRITE_REG(0x53,52+(at>>13));
        *((uint8_t *)(0x6000u+(at&8191)))=agi_pixel_get(at);
    }
}
static uint8_t matches_reference(void) {
    uint16_t at;
    for (at=0; at<AGI_PIXELS; ++at) {
        if (!(at & 8191)) ZXN_WRITE_REG(0x53,52+(at>>13));
        if (*((uint8_t *)(0x6000u+(at&8191)))!=agi_pixel_get(at)) return 0;
    }
    return 1;
}
static uint8_t display_matches(uint8_t priority) {
    uint8_t page,y,x,p;
    uint16_t at;
    const uint8_t *display;
    next_show_picture(priority);
    for(page=0;page<10;++page) {
        ZXN_WRITE_REG(0x53,40+page);
        for(y=0;y<AGI_HEIGHT;++y) {
            at=(uint16_t)y*160+(uint16_t)page*16;
            for(x=0;x<16;++x) {
                p=agi_pixel_get(at+x); p=priority?p>>4:p&15;
                display=(const uint8_t *)(0x6000u+(uint16_t)x*512+y+40);
                if(display[0]!=p || display[256]!=p) return 0;
            }
        }
    }
    return 1;
}
static const uint8_t enclosed[] = {0xf0,0,0xf6,10,10,20,10,20,20,10,20,10,10,0xf0,2,0xf2,9,0xf8,15,15,0xff};
static const uint8_t priority_only[] = {0xf2,6,0xf8,0,0,0xff};
static const uint8_t white_fill[] = {0xf0,15,0xf8,0,0,0xff};
static const uint8_t short_line[] = {0xf0,1,0xf6,10,10,12,11,0xff};
static const uint8_t reverse_line[] = {0xf0,1,0xf6,12,11,10,10,0xff};
static const uint8_t malformed[] = {0xf0,1,0xf6,10,0xff};
static const uint8_t no_end[] = {0xf0,1};
static const uint8_t bad_opcode[] = {0xfb,0xff};
static const uint8_t bad_coord[] = {0xf0,1,0xf6,160,0,0xff};
static const uint8_t brush[] = {0xf9,1,0xfa,20,20,0xff};

static void log_line(const char *s) {
    if (esx_f_write(log_handle, (void *)s, strlen(s)) != strlen(s)) {
        next_text(0, 28, "TEST LOG WRITE FAILED");
        for (;;) {}
    }
    esx_f_sync(log_handle);
}
static void check(uint8_t ok, const char *name) {
    if (!ok) ++failures;
    sprintf(message, "%s %s\n", ok ? "PASS" : "FAIL", name); log_line(message);
}
static uint8_t decode(const uint8_t *p, uint16_t n) {
    agi_picture_clear(); return agi_picture_decode(p, n);
}
int main(void) {
    unsigned int n, loaded = 0;
    uint16_t length, at;
    uint8_t result, h, reference_result;
    next_init(); next_text(0, 3, "ZXQUEST RENDERER TESTS RUNNING");
    errno = 0;
    log_handle = esx_f_open("build/verify.txt", ESX_MODE_WRITE | ESX_MODE_OPEN_CREAT_TRUNC);
    if (errno) { next_text(0, 5, "CANNOT OPEN build/verify.txt"); for (;;) {} }
    check(decode(enclosed,sizeof(enclosed)) == PIC_OK && agi_pixel_get(15*160+15) == 0x92 && agi_pixel_get(0) == 0x4f && agi_pixel_get(10*160+10) == 0x40, "enclosed visual and priority fill");
    result = decode(priority_only,sizeof(priority_only));
    for (at=0; at<AGI_PIXELS && agi_pixel_get(at)==0x6f; ++at) {}
    check(result == PIC_OK && at == AGI_PIXELS, "full-screen priority fill across banks");
    check(decode(white_fill,sizeof(white_fill)) == PIC_OK && agi_pixel_get(0)==0x4f, "white fill terminates");
    check(decode(short_line,sizeof(short_line)) == PIC_OK && agi_pixel_get(11*160+11)==0x41 && agi_pixel_get(10*160+11)==0x4f, "positive midpoint rounding");
    check(decode(reverse_line,sizeof(reverse_line)) == PIC_OK && agi_pixel_get(10*160+11)==0x41, "negative midpoint rounding");
    check(decode(malformed,sizeof(malformed)) == PIC_TRUNCATED, "truncated coordinate rejected");
    check(decode(no_end,sizeof(no_end)) == PIC_TRUNCATED, "missing terminator rejected");
    check(decode(bad_opcode,sizeof(bad_opcode)) == PIC_BAD_OPCODE, "unknown opcode rejected");
    check(decode(bad_coord,sizeof(bad_coord)) == PIC_BAD_ARGUMENT, "out-of-range coordinate rejected");
    check(decode(brush,sizeof(brush)) == PIC_UNSUPPORTED_BRUSH, "unsupported brush reported");
    result = resource_load("games/sq1","PICDIR",1,data,1,&length);
    check(result == RES_TOO_LARGE, "resource capacity enforced");
    result = resource_load("games/missing","PICDIR",1,data,sizeof(data),&length);
    check(result == RES_IO, "missing game directory reported");
    for (n = 0; n < 256; ++n) {
        result = resource_load("games/sq1","PICDIR",n,data,sizeof(data),&length);
        if (result == RES_MISSING) continue;
        if (!result) {
            agi_picture_clear(); reference_result=agi_picture_decode_reference(data,length);
            save_reference();
            agi_picture_clear(); result = agi_picture_decode(data,length);
            if (reference_result!=result || !matches_reference()) {
                ++failures;
                sprintf(message,"FAIL PIC %u differs from original decoder\n",n); log_line(message);
            }
        }
        if (result) ++failures; else ++loaded;
        sprintf(message,"PIC %u bytes %u result %u\n",n,length,(unsigned int)result); log_line(message);
        if (!result) {
            if(n==1 || n==27 || n==67) {
                check(display_matches(0),"visual framebuffer matches decoded pixels");
                check(display_matches(1),"priority framebuffer matches decoded pixels");
            }
            next_show_picture(0); sprintf(message,"PICTURE %03u                      ", n); next_text(0,3,message);
        }
    }
    /* Save target-decoded pixels for a reproducible visual artifact (PIC 1). */
    result = resource_load("games/sq1","PICDIR",1,data,sizeof(data),&length);
    if (!result) {
        agi_picture_clear(); result = agi_picture_decode(data,length);
        if (!result) {
            errno=0; h=esx_f_open("build/pic001.raw",ESX_MODE_WRITE | ESX_MODE_OPEN_CREAT_TRUNC);
            if (!errno) {
                for (at=0;at<AGI_PIXELS;) {
                    length=AGI_PIXELS-at; if(length>sizeof(data)) length=sizeof(data);
                    for(n=0;n<length;++n) data[n]=agi_pixel_get(at+n);
                    if(esx_f_write(h,data,length)!=length) { ++failures; break; }
                    at+=length;
                }
                esx_f_close(h);
            } else ++failures;
        }
    }
    sprintf(message,"SUMMARY pictures=%u failures=%u\n",loaded,failures); log_line(message);
    esx_f_close(log_handle);
    next_show_picture(0);
    next_text(0,3, failures ? "TESTS FAILED - SEE build/verify.txt" : "ALL TESTS PASSED");
    for (;;) {}
}
