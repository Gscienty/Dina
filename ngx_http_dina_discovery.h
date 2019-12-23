#ifndef _NGX_HTTP_DINA_DISCOVERY_H
#define _NGX_HTTP_DINA_DISCOVERY_H

#include <ngx_core.h>

typedef struct ngx_http_dina_config_param_s ngx_http_dina_config_param_t;
struct ngx_http_dina_config_param_s {
    ngx_array_t *lengths;
    ngx_array_t *values;
    ngx_str_t static_param;
};

typedef struct ngx_http_dina_zoo_config_s ngx_http_dina_zoo_config_t;
struct ngx_http_dina_zoo_config_s {
    ngx_str_t addr;

    ngx_http_dina_config_param_t service;
};

int ngx_http_dina_discovery(ngx_str_t *const result, const ngx_http_dina_zoo_config_t *const zoo_config, const ngx_str_t *const service_name);

#endif
