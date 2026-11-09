#include <arch/zxn.h>
#include <arch/zxn/esxdos.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "agi/picture.h"
#include "next/platform.h"
#include "next/view_resource.h"
#include "../build/view_manifest.h"

static const uint8_t valid[] = {1,1,2,0,0,9,0,9,0,1,3,0,4,2,0x8f,0x12,0x21,0,0xf1,0x32,0};
static const uint8_t expected_pixels[] = {1,1,2,15,15,3,3,15};
static const uint8_t expected_mirror[] = {15,2,1,1,15,3,3,15};
static uint8_t data[64], decoded[8], fail_read, row_count;
static uint8_t picture_data[4096];
static char message[100];
static uint8_t log_file;
static uint16_t checks, failures, views, cels, mirrored, crc;
static agi_view_source source;
static agi_view_cel cel;
static next_view resource;
static const uint16_t crc_table[16] = {
    0,0x1021,0x2042,0x3063,0x4084,0x50a5,0x60c6,0x70e7,
    0x8108,0x9129,0xa14a,0xb16b,0xc18c,0xd1ad,0xe1ce,0xf1ef
};
static void log_line(const char *line) {
    if(esx_f_write(log_file,(void *)line,strlen(line))!=strlen(line)) {
        next_text(0,28,"VIEW TEST LOG WRITE FAILED"); for(;;) {}
    }
}
static void check(uint8_t ok, const char *name) {
    ++checks; if(!ok) ++failures;
    sprintf(message,"%s %s\n",ok ? "PASS" : "FAIL",name); log_line(message);
}
static uint8_t memory_read(void *context, uint16_t at, void *target, uint16_t count) {
    if(fail_read) return 1;
    memcpy(target,(uint8_t *)context+at,count); return 0;
}
static void collect(void *context, uint8_t row, const uint8_t *pixels, uint8_t width) {
    (void)context;
    if(width==4 && row<2) memcpy(decoded+row*4,pixels,4);
    ++row_count;
}
static void checksum(void *context, uint8_t row, const uint8_t *pixels, uint8_t width) {
    uint16_t i;
    (void)context; (void)row;
    for(i=0; i<width; ++i) {
        crc=(crc<<4)^crc_table[(crc>>12)^(pixels[i]>>4)];
        crc=(crc<<4)^crc_table[(crc>>12)^(pixels[i]&15)];
    }
}
static void reset_source(void) {
    memcpy(data,valid,sizeof(valid));
    source.context=data; source.length=sizeof(valid); source.read=memory_read;
    fail_read=row_count=0; memset(decoded,0,sizeof(decoded));
}
static void decoder_tests(void) {
    uint8_t result;
    agi_view_info info;
    reset_source();
    check(!agi_view_inspect(&source,&info) && info.loops==2 && !info.description, "VIEW header and loop table");
    result=agi_view_select(&source,0,0,&cel);
    check(!result && cel.width==4 && cel.height==2 && cel.transparent==15 && !cel.mirrored &&
          !agi_view_decode(&source,&cel,collect,0) && row_count==2 && !memcmp(decoded,expected_pixels,8),
          "RLE rows and implicit transparent tail");
    result=agi_view_select(&source,1,0,&cel); row_count=0;
    check(!result && cel.mirrored && !agi_view_decode(&source,&cel,collect,0) && !memcmp(decoded,expected_mirror,8),
          "shared cel mirrored only in the other loop");
    check(agi_view_select(&source,2,0,&cel)==VIEW_BAD_INDEX && agi_view_select(&source,0,1,&cel)==VIEW_BAD_INDEX,
          "loop and cel index bounds");
    source.length=4;
    check(agi_view_inspect(&source,&info)==VIEW_TRUNCATED, "short VIEW header");
    reset_source(); data[2]=0;
    check(agi_view_inspect(&source,&info)==VIEW_BAD_HEADER, "empty loop table rejected");
    reset_source(); data[2]=255;
    check(agi_view_inspect(&source,&info)==VIEW_TRUNCATED, "truncated loop offsets rejected");
    reset_source(); data[3]=255; data[4]=255;
    check(agi_view_inspect(&source,&info)==VIEW_BAD_HEADER, "description offset bounds");
    reset_source(); data[5]=0;
    check(agi_view_select(&source,0,0,&cel)==VIEW_BAD_HEADER, "loop offset cannot point into header");
    reset_source(); data[9]=255;
    check(agi_view_select(&source,0,0,&cel)==VIEW_TRUNCATED, "truncated cel table rejected");
    reset_source(); data[10]=0;
    check(agi_view_select(&source,0,0,&cel)==VIEW_BAD_HEADER, "cel offset cannot point into loop table");
    reset_source(); data[12]=0;
    check(agi_view_select(&source,0,0,&cel)==VIEW_BAD_HEADER, "zero cel dimensions rejected");
    reset_source(); agi_view_select(&source,0,0,&cel); data[15]=0x15;
    check(agi_view_decode(&source,&cel,0,0)==VIEW_BAD_RLE, "row cannot exceed cel width");
    data[15]=0x10;
    check(agi_view_decode(&source,&cel,0,0)==VIEW_BAD_RLE, "zero-length nonzero RLE token rejected");
    reset_source(); agi_view_select(&source,0,0,&cel); --source.length;
    check(agi_view_decode(&source,&cel,0,0)==VIEW_TRUNCATED, "missing last row terminator");
    reset_source(); fail_read=1;
    check(agi_view_select(&source,0,0,&cel)==VIEW_SOURCE_ERROR, "reader errors propagated");
    reset_source();
    resource_cache_init("build/view-fixtures");
    result=next_view_open(&resource,0);
    check(!result && !agi_view_select(&resource.source,0,0,&cel) && !agi_view_decode(&resource.source,&cel,collect,0) &&
          !memcmp(decoded,expected_pixels,8), "banked RLE crosses 8 KB boundary");
    next_view_close(&resource);
    result=next_view_open(&resource,1);
    check(!result && !agi_view_select(&resource.source,0,0,&cel) && !agi_view_decode(&resource.source,&cel,collect,0) &&
          !memcmp(decoded,expected_pixels,8), "banked cel header crosses 8 KB boundary");
    next_view_close(&resource);
    result=next_view_open(&resource,2);
    check(!result && agi_view_select(&resource.source,0,0,&cel)==VIEW_TRUNCATED, "16-bit relative cel offset cannot wrap");
    next_view_close(&resource);
}
static uint8_t pixel(uint8_t x, uint8_t y) { return agi_pixel_get((uint16_t)y*160+x); }
static void put(uint8_t x, uint8_t y, uint8_t value) { agi_pixel_set((uint16_t)y*160+x,value); }
static uint8_t display_matches(uint8_t priority) {
    uint8_t page, y, x, p;
    uint16_t at;
    const uint8_t *display;
    for(page=0; page<10; ++page) {
        ZXN_WRITE_REG(0x53,40+page);
        for(y=0; y<168; ++y) {
            at=(uint16_t)y*160+(uint16_t)page*16;
            for(x=0; x<16; ++x) {
                p=agi_pixel_get(at+x); p=priority ? p>>4 : p&15;
                display=(const uint8_t *)(0x6000u+(uint16_t)x*512+y+next_picture_top);
                if(display[0]!=p || display[256]!=p) return 0;
            }
        }
    }
    return 1;
}
/* Deliberately uncached pixel-at-a-time reference for priority look-down.
   Decode is shared, but the production row compositor is never called. */
typedef struct { int16_t x,top; uint8_t priority; } reference_position;
static void reference_row(void *context,uint8_t row,const uint8_t *pixels,uint8_t width) {
    reference_position *p=context; int16_t x,y=p->top+row;
    uint16_t at,below;uint8_t i,packed,priority;
    if(y<0 || y>=168)return;
    for(i=0;i<width;++i) {
        x=p->x+i;if(x<0 || x>=160 || pixels[i]==15)continue;
        at=(uint16_t)y*160+x;packed=agi_pixel_get(at);priority=packed>>4;
        if(priority<3) {
            below=at;
            do {below+=160;priority=below<AGI_PIXELS?agi_pixel_get(below)>>4:4;}while(priority<3);
        }
        if(p->priority>=priority)agi_pixel_set(at,(packed<0x30?packed&0xf0:p->priority<<4)|pixels[i]);
    }
}
static void control_background(void) {
    uint16_t at;uint8_t y,x,p;
    /* Repeated runs, transparent normal-row separators, water, and runs that
       reach the picture bottom. Rows 51/102/153 straddle bank boundaries. */
    for(y=0;y<168;++y) {
        for(x=0;x<160;++x) {
            if((x&3)==2)p=2;
            else if(y%7==1)p=(x&3)==0?12:(x&3)==1?4:3;
            else p=(x+y)%3;
            picture_data[x]=(p<<4)|14;
        }
        at=(uint16_t)y*160;agi_pixels_write(at,picture_data,160);
    }
}
static uint16_t picture_crc(void) {
    uint16_t at,n,i,result=0xffff;uint8_t b;
    for(at=0;at<AGI_PIXELS;at+=n) {
        n=AGI_PIXELS-at;if(n>sizeof(picture_data))n=sizeof(picture_data);
        agi_pixels_read(at,picture_data,n);
        for(i=0;i<n;++i){b=picture_data[i];result=(result<<4)^crc_table[(result>>12)^(b>>4)];result=(result<<4)^crc_table[(result>>12)^(b&15)];}
    }
    return result;
}
static void control_blit_tests(void) {
    static const uint8_t widths[]={4,16,17,40};
    uint8_t c,row,n,run,at,r,ok=1;uint16_t expected;reference_position p;
    for(c=0;c<8;++c) {
        reset_source();data[12]=widths[c&3];data[13]=8;at=15;
        for(row=0;row<8;++row) {
            if(row!=2) {
                data[at++]=0x21;data[at++]=0xf2;n=widths[c&3]-3;
                while(n){run=n>15?15:n;data[at++]=((row%4+1)<<4)|run;n-=run;}
            }
            data[at++]=0;
        }
        source.length=at;r=agi_view_select(&source,c&1,0,&cel);
        p.x=c==0?-2:c==3?152:14;p.top=c>=4?164:48;p.priority=8;
        control_background();if(!r)r=agi_view_decode(&source,&cel,reference_row,&p);expected=picture_crc();
        control_background();if(!r)r=agi_view_blit(&source,&cel,p.x,p.top+7,p.priority);
        if(r || picture_crc()!=expected)ok=0;
    }
    check(ok,"packed-priority blits match reference across control runs clipping mirrors and banks");
}
static void upload_tests(void) {
    uint16_t at,n,i;uint8_t x,width,saved=ZXN_READ_REG(0x53),ok;
    for(at=0;at<AGI_PIXELS;at+=n) {
        n=AGI_PIXELS-at;if(n>sizeof(picture_data))n=sizeof(picture_data);
        for(i=0;i<n;++i)picture_data[i]=(at+i)*17+((at+i)>>8);
        agi_pixels_write(at,picture_data,n);
    }
    next_picture_top=32;
    next_show_rect(-32767,-32767,65535u,65535u,0);ok=ZXN_READ_REG(0x53)==saved;
    check(ok&&display_matches(0),"full clipped visual upload matches all pixels at shifted picture origin");
    ZXN_WRITE_REG(0x53,saved);next_picture_top=88;
    for(x=0;x<160;x+=width){width=x%17+1;if(width>160-x)width=160-x;next_show_rect(x,0,width,168,1);}
    ok=ZXN_READ_REG(0x53)==saved;
    check(ok&&display_matches(1),"variable-width priority uploads match all pixels through final display row");
    next_picture_top=40;ZXN_WRITE_REG(0x53,saved);
}
static void compositor_tests(void) {
    uint16_t at;
    uint8_t result, ok;
    reset_source(); agi_view_select(&source,0,0,&cel);
    agi_picture_clear(); put(12,10,0x79); put(11,11,0x2e); put(11,12,0x7f); put(12,11,0x3e);
    result=agi_view_blit(&source,&cel,10,11,6);
    check(!result && pixel(10,10)==0x61 && pixel(12,10)==0x79 && pixel(13,10)==0x4f &&
          pixel(10,11)==0x4f && pixel(11,11)==0x2e && pixel(12,11)==0x63, "transparency foreground occlusion and water priority");
    put(11,12,0x4f);
    result=agi_view_blit(&source,&cel,10,11,6);
    check(!result && pixel(11,11)==0x23, "control-line effective priority and preserved control data");
    agi_picture_clear(); agi_view_blit(&source,&cel,10,11,9); agi_view_blit(&source,&cel,11,11,6);
    check(pixel(11,10)==0x91 && pixel(13,10)==0x62, "overlapping sprite priorities");
    agi_picture_clear(); agi_view_blit(&source,&cel,10,11,6); agi_view_blit(&source,&cel,11,11,6);
    check(pixel(12,10)==0x61, "later sprite wins equal priority");
    agi_picture_clear(); agi_pixel_set(AGI_PIXELS,0xaa);
    result=agi_view_blit(&source,&cel,-2,0,6);
    check(!result && pixel(0,0)==0x63 && pixel(1,0)==0x4f, "negative left and top clipping");
    result=agi_view_blit(&source,&cel,159,168,6);
    check(!result && pixel(159,167)==0x61 && agi_pixel_get(AGI_PIXELS)==0xaa, "right and bottom clipping with guard byte");
    check(!agi_view_blit(&source,&cel,-32767,32767,6) && pixel(0,0)==0x63,
          "extreme coordinates do not overflow");
    put(11,167,0x0e);
    check(!agi_view_blit(&source,&cel,10,167,6) && pixel(11,167)==0x03,
          "control line at bottom of picture is bounded");
    agi_picture_clear(); data[19]=0x35;
    check(agi_view_blit(&source,&cel,10,11,6)==VIEW_BAD_RLE && pixel(10,10)==0x4f,
          "malformed second row does not draw first row");
    reset_source(); agi_view_select(&source,0,0,&cel);
    check(agi_view_priority(0)==4 && agi_view_priority(47)==4 && agi_view_priority(48)==5 &&
          agi_view_priority(167)==14 && agi_view_priority(255)==14, "default AGI priority bands");

    for(at=0; at<AGI_PIXELS; ++at) agi_pixel_set(at,(uint8_t)(at*17+(at>>8)));
    next_scene_capture();
    for(at=8160; at<8320; ++at) agi_pixel_set(at,0);
    next_scene_restore(-5,51,170,1); ok=1;
    for(at=0; at<AGI_PIXELS; ++at) if(agi_pixel_get(at)!=(uint8_t)(at*17+(at>>8))) { ok=0; break; }
    check(ok, "background restoration crosses bank and clips rectangle");
    agi_picture_clear(); next_scene_restore(-32767,-32767,65535u,65535u); ok=1;
    for(at=0; at<AGI_PIXELS; ++at) if(agi_pixel_get(at)!=(uint8_t)(at*17+(at>>8))) { ok=0; break; }
    check(ok, "large restore rectangle cannot overflow");

    agi_picture_clear(); next_scene_capture(); next_show_picture(0);
    ZXN_WRITE_REG(0x53,40); *(uint8_t *)0x6000=0xab;
    agi_view_blit(&source,&cel,14,52,6); next_show_rect(14,51,4,2,0);
    check(display_matches(0), "dirty display spans picture and Layer 2 bank boundaries");
    next_scene_restore(14,51,4,2);
    agi_view_blit(&source,&cel,15,53,6);
    next_show_rect(14,51,4,2,0); next_show_rect(15,52,4,2,0);
    check(display_matches(0), "moving sprite restores old pixels without trails");
    next_show_picture(1); next_scene_restore(15,52,4,2);
    agi_view_blit(&source,&cel,16,54,6);
    next_show_rect(15,52,4,2,1); next_show_rect(16,53,4,2,1);
    check(display_matches(1), "dirty display preserves priority mode");
    ZXN_WRITE_REG(0x53,40);
    check(*(uint8_t *)0x6000==0xab, "scene updates leave HUD pixels unchanged");
}
static void game_tests(void) {
    uint8_t file, expected[11], current=0, opened=0, result;
    uint16_t count, wanted_crc;
    errno=0; file=esx_f_open("build/view-expect.bin",ESX_MODE_READ | ESX_MODE_OPEN_EXIST);
    if(errno) { check(0,"open host VIEW expectations"); return; }
    resource_cache_init("games/sq1");
    for(;;) {
        count=esx_f_read(file,expected,sizeof(expected));
        if(!count) break;
        if(count!=sizeof(expected)) { check(0,"complete expectation record"); break; }
        if(!opened || expected[0]!=current) {
            if(opened) next_view_close(&resource);
            result=next_view_open(&resource,expected[0]);
            if(result) { check(0,"open game VIEW"); break; }
            current=expected[0]; opened=1; ++views;
        }
        result=agi_view_select(&resource.source,expected[1],expected[2],&cel);
        crc=0xffff;
        if(!result) result=agi_view_decode(&resource.source,&cel,checksum,0);
        wanted_crc=expected[9]|((uint16_t)expected[10]<<8);
        if(result || cel.loops!=expected[3] || cel.cels!=expected[4] || cel.width!=expected[5] ||
           cel.height!=expected[6] || cel.transparent!=expected[7] || cel.mirrored!=expected[8] || crc!=wanted_crc) {
            ++failures;
            sprintf(message,"FAIL VIEW %u loop %u cel %u result %u CRC %04x expected %04x\n",
                (unsigned)current,(unsigned)expected[1],(unsigned)expected[2],(unsigned)result,crc,wanted_crc); log_line(message);
        } else { ++cels; mirrored+=cel.mirrored; }
        if(!(cels&127)) { sprintf(message,"CELS VERIFIED %u              ",cels); next_text(0,5,message); }
    }
    if(opened) next_view_close(&resource);
    esx_f_close(file);
    check(views==EXPECTED_VIEWS && cels==EXPECTED_CELS && mirrored==EXPECTED_MIRRORED,"all game cels and mirror cases verified");
}
static void export_room(void) {
    uint16_t length, at, i, chunk;
    uint8_t result, file;
    result=resource_load("games/sq1","PICDIR",1,picture_data,sizeof(picture_data),&length);
    if(!result) { agi_picture_clear(); result=agi_picture_decode(picture_data,length); }
    if(!result) result=next_view_open(&resource,1);
    if(!result) result=agi_view_select(&resource.source,0,0,&cel);
    if(!result) { next_scene_capture(); result=agi_view_blit(&resource.source,&cel,76,140,agi_view_priority(140)); }
    next_view_close(&resource);
    check(!result,"real room and Roger composited");
    if(result) return;
    errno=0; file=esx_f_open("build/sprite-room.raw",ESX_MODE_WRITE | ESX_MODE_OPEN_CREAT_TRUNC);
    if(errno) { check(0,"open sprite room export"); return; }
    for(at=0; at<AGI_PIXELS; at+=chunk) {
        chunk=AGI_PIXELS-at; if(chunk>sizeof(picture_data)) chunk=sizeof(picture_data);
        for(i=0; i<chunk; ++i) picture_data[i]=agi_pixel_get(at+i);
        if(esx_f_write(file,picture_data,chunk)!=chunk) { check(0,"write sprite room export"); break; }
    }
    esx_f_close(file); next_show_picture(0);
}
int main(void) {
    next_init(); next_text(0,3,"ZXQUEST VIEW AND COMPOSITOR TESTS");
    errno=0; log_file=esx_f_open("build/view-tests.txt",ESX_MODE_WRITE | ESX_MODE_OPEN_CREAT_TRUNC);
    if(errno) { next_text(0,5,"CANNOT OPEN VIEW TEST LOG"); for(;;) {} }
    decoder_tests(); compositor_tests(); control_blit_tests(); upload_tests(); game_tests(); export_room();
    sprintf(message,"SUMMARY views=%u cels=%u mirrored=%u checks=%u failures=%u\n",views,cels,mirrored,checks,failures);
    log_line(message); esx_f_close(log_file);
    next_text(0,3,failures ? "VIEW TESTS FAILED" : "VIEW TESTS PASSED");
    sprintf(message,"VIEWS %u  CELS %u  ERRORS %u",views,cels,failures); next_text(0,28,message);
    for(;;) {}
}
