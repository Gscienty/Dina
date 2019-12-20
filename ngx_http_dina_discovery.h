#ifndef _NGX_HTTP_DINA_DISCOVERY_H
#define _NGX_HTTP_DINA_DISCOVERY_H

#include <ngx_core.h>

int ngx_http_dina_discovery(ngx_str_t *const result, const char *const host, const char *const path);

#endif
