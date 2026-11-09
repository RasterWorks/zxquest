#include "view_resource.h"
/* Callback stays resident while VIEW decoding changes code banks. */
extern uint8_t next_view_read(void *,uint16_t,void *,uint16_t);
uint8_t next_view_open(next_view *view, uint8_t number) {
    uint8_t result=resource_acquire(RESOURCE_VIEW,number,&view->handle);
    view->source.length=0;
    view->source.context=&view->handle;
    view->source.read=next_view_read;
    if(!result) resource_size(view->handle,&view->source.length);
    return result;
}
void next_view_close(next_view *view) {
    if(view->handle) resource_release(view->handle);
    view->handle=0; view->source.length=0;
}
