#include "service.h"

const struct http_service* http_service_findServiceEntry(const char* resource)
{
    if(resource == NULL) return NULL;
    int i;
    for(i = 0; i < HTTP_SERVICE_NUM; i++)
    {
        if(strcmp(services[i].service_path, resource) == 0)
            return &services[i];
    }
    return NULL;
}