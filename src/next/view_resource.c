#include "view_resource.h"
uint8_t next_view_read(void *context, uint16_t at, void *data, uint16_t count) {
    return resource_copy(*(resource_handle *)context,at,data,count);
}
