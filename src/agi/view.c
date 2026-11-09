#include <string.h>
#include "view.h"
#include "picture.h"

static uint8_t input[64], pixels[255];
static uint16_t stream_at;
static uint8_t input_at, input_size;

static uint8_t read_at(const agi_view_source *source, uint16_t at, void *data, uint16_t count) {
    if (at > source->length || count > source->length-at) return VIEW_TRUNCATED;
    return source->read(source->context,at,data,count) ? VIEW_SOURCE_ERROR : VIEW_OK;
}
static uint16_t word(const uint8_t *p) { return p[0] | ((uint16_t)p[1] << 8); }
uint8_t agi_view_inspect(const agi_view_source *source, agi_view_info *info) {
    uint8_t header[5], result;
    result=read_at(source,0,header,5);
    if(result) return result;
    info->loops=header[2]; info->description=word(header+3);
    if(!info->loops) return VIEW_BAD_HEADER;
    if(5u+2u*info->loops > source->length) return VIEW_TRUNCATED;
    if(info->description && (info->description < 5u+2u*info->loops || info->description >= source->length)) return VIEW_BAD_HEADER;
    return VIEW_OK;
}
static uint8_t select_uncached(const agi_view_source *source, uint8_t loop, uint8_t cel, agi_view_cel *selected) {
    agi_view_info info;
    uint8_t header[3], count, result;
    uint16_t loop_at, relative, at;
    result=agi_view_inspect(source,&info); if(result) return result;
    if(loop >= info.loops) return VIEW_BAD_INDEX;
    result=read_at(source,5u+2u*loop,header,2); if(result) return result;
    loop_at=word(header);
    if(loop_at < 5u+2u*info.loops) return VIEW_BAD_HEADER;
    result=read_at(source,loop_at,&count,1); if(result) return result;
    if(!count) return VIEW_BAD_HEADER;
    if(1u+2u*count > source->length-loop_at) return VIEW_TRUNCATED;
    if(cel >= count) return VIEW_BAD_INDEX;
    result=read_at(source,loop_at+1u+2u*cel,header,2); if(result) return result;
    relative=word(header);
    if(relative < 1u+2u*count) return VIEW_BAD_HEADER;
    if(relative > source->length-loop_at) return VIEW_TRUNCATED;
    at=loop_at+relative;
    result=read_at(source,at,header,3); if(result) return result;
    if(!header[0] || !header[1]) return VIEW_BAD_HEADER;
    selected->offset=at+3;
    selected->loops=info.loops; selected->cels=count;
    selected->width=header[0]; selected->height=header[1];
    selected->transparent=header[2]&15;
    selected->mirrored=(header[2]&128) && ((header[2]>>4)&7)!=loop;
    return VIEW_OK;
}
/* Only the banked Next runtime supplies immutable, generation-tagged
   resource handles. Generic readers retain full validation on every call. */
#ifdef VIEW_RESOURCE_CACHE
#include <arch/zxn.h>
#include "../next/resource_cache.h"
extern uint8_t next_view_read(void *,uint16_t,void *,uint16_t);
typedef struct {resource_handle handle;uint16_t length;uint8_t loop,cel,valid;agi_view_cel info;} cel_cache;
/* Spare tail of clean-picture page 39, after text masks. Cache keys include
   the complete handle generation; restored old padding lacks the marker. */
static cel_cache *cel_entries=(cel_cache *)0x7900;
static uint8_t cel_hint;
static uint8_t cache_slot(const agi_view_source *s,uint8_t loop,uint8_t cel) {
    resource_handle handle=*(resource_handle *)s->context;
    return (uint8_t)(handle^(handle>>8)^((uint16_t)loop*7)^cel)&31;
}
#endif
uint8_t agi_view_select(const agi_view_source *source,uint8_t loop,uint8_t cel,agi_view_cel *selected) {
    uint8_t r;
#ifdef VIEW_RESOURCE_CACHE
    uint8_t saved,slot;resource_handle handle;cel_cache *entry;
    if(source->read==next_view_read) {
        handle=*(resource_handle *)source->context;slot=cache_slot(source,loop,cel);
        saved=ZXN_READ_REG(0x53);ZXN_WRITE_REG(0x53,39);
        if(memcmp((void *)0x7ff0,"CEL1",4)){memset(cel_entries,0,32*sizeof(cel_cache));memcpy((void *)0x7ff0,"CEL1",4);}
        entry=cel_entries+slot;cel_hint=slot;
        if(handle && entry->handle==handle && entry->length==source->length && entry->loop==loop && entry->cel==cel) {
            memcpy(selected,&entry->info,sizeof(*selected));ZXN_WRITE_REG(0x53,saved);return VIEW_OK;
        }
        ZXN_WRITE_REG(0x53,saved);
        r=select_uncached(source,loop,cel,selected);if(r)return r;
        ZXN_WRITE_REG(0x53,39);entry->handle=handle;entry->length=source->length;entry->loop=loop;entry->cel=cel;
        entry->valid=0;memcpy(&entry->info,selected,sizeof(*selected));ZXN_WRITE_REG(0x53,saved);return VIEW_OK;
    }
#endif
    r=select_uncached(source,loop,cel,selected);return r;
}
static uint8_t validate(const agi_view_source *source,const agi_view_cel *cel) {
    uint8_t r;
#ifdef VIEW_RESOURCE_CACHE
    uint8_t saved=0,match=0;cel_cache *entry=cel_entries;
    if(source->read==next_view_read) {
        saved=ZXN_READ_REG(0x53);ZXN_WRITE_REG(0x53,39);entry=cel_entries+cel_hint;
        match=!memcmp((void *)0x7ff0,"CEL1",4) && entry->handle && entry->handle==*(resource_handle *)source->context && entry->length==source->length && !memcmp(&entry->info,cel,sizeof(*cel));
        r=match && entry->valid;ZXN_WRITE_REG(0x53,saved);if(r)return VIEW_OK;
    }
    r=agi_view_decode(source,cel,0,0);
    if(!r && match){ZXN_WRITE_REG(0x53,39);entry->valid=1;ZXN_WRITE_REG(0x53,saved);}
#else
    r=agi_view_decode(source,cel,0,0);
#endif
    return r;
}
static uint8_t next_byte(const agi_view_source *source, uint8_t *value) {
    uint16_t remaining;
    uint8_t result;
    if(input_at == input_size) {
        if(stream_at >= source->length) return VIEW_TRUNCATED;
        remaining=source->length-stream_at;
        input_size=remaining < sizeof(input) ? remaining : sizeof(input);
        result=read_at(source,stream_at,input,input_size); if(result) return result;
        stream_at+=input_size; input_at=0;
    }
    *value=input[input_at++];
    return VIEW_OK;
}
uint8_t agi_view_decode(const agi_view_source *source, const agi_view_cel *cel, agi_view_row row, void *context) {
    uint16_t x;
    uint8_t y, value, run, result, i, temporary;
    if(!cel->width || !cel->height || cel->transparent>15) return VIEW_BAD_ARGUMENT;
    stream_at=cel->offset; input_at=input_size=0;
    for(y=0; y<cel->height; ++y) {
        x=0;
        if(row) memset(pixels,cel->transparent,cel->width);
        for(;;) {
            result=next_byte(source,&value); if(result) return result;
            if(!value) break;
            run=value&15;
            if(!run || x+run > cel->width) return VIEW_BAD_RLE;
            if(row) memset(pixels+x,value>>4,run);
            x+=run;
        }
        if(row) {
            if(cel->mirrored) for(i=0; i<cel->width/2; ++i) {
                temporary=pixels[i]; pixels[i]=pixels[cel->width-1-i]; pixels[cel->width-1-i]=temporary;
            }
            row(context,y,pixels,cel->width);
        }
    }
    return VIEW_OK;
}

typedef struct { int16_t x, top; uint8_t priority, transparent; } blit_context;
static void blit_row(void *context, uint8_t row, const uint8_t *data, uint8_t width) {
    blit_context *b=context;
    int16_t left=b->x,right=b->x+width,y=b->top+row;
    uint16_t at, below, pixel_at;
    uint8_t n,count,packed,priority=b->priority<<4,limit=priority|15;
    uint8_t transparent=b->transparent,effective,colour,background[32],*target;
    const uint8_t *source;
    if(y<0 || y>=AGI_HEIGHT) return;
    if(left<0)left=0;if(right>AGI_WIDTH)right=AGI_WIDTH;if(right<=left)return;
    source=data+(left-b->x);at=(uint16_t)y*AGI_WIDTH+left;
    while(left<right) {
        n=right-left;if(n>sizeof(background))n=sizeof(background);
        agi_pixels_read(at,background,n);
        target=background;pixel_at=at;count=n;
        do {
            colour=*source++;
            if(colour!=transparent) {
                packed=*target;
                /* Compare packed bytes against a priority with low bits set:
                   this preserves equal-priority wins without per-pixel shifts. */
                if(packed>=0x30) {
                    if(packed<=limit)*target=priority|colour;
                } else {
                    below=pixel_at;
                    do {
                        below+=AGI_WIDTH;
                        effective=below<AGI_PIXELS ? agi_pixel_get(below) : 0x40;
                    } while(effective<0x30);
                    if(effective<=limit)*target=(packed&0xf0)|colour;
                }
            }
            ++target;++pixel_at;
        }while(--count);
        agi_pixels_write(at,background,n);left+=n;at+=n;
    }
}
uint8_t agi_view_blit(const agi_view_source *source, const agi_view_cel *cel, int16_t x, int16_t baseline, uint8_t priority) {
    blit_context context;
    uint8_t result;
    if(priority>15) return VIEW_BAD_ARGUMENT;
    result=validate(source,cel); if(result) return result;
    if(x<=-255 || x>=AGI_WIDTH || baseline<0 || baseline>=AGI_HEIGHT+255) return VIEW_OK;
    context.x=x; context.top=baseline-cel->height+1;
    context.priority=priority; context.transparent=cel->transparent;
    return agi_view_decode(source,cel,blit_row,&context);
}
uint8_t agi_view_priority(uint8_t baseline) {
    uint8_t priority=baseline/12+1;
    if(priority<4) return 4;
    return priority>14 ? 14 : priority;
}
