#ifndef _NGX_HTTP_DINA_CONF_H
#define _NGX_HTTP_DINA_CONF_H

#include <ngx_core.h>
#include <ngx_http.h>
#include "ngx_http_dina_discovery.h"

typedef struct ngx_http_dina_module_srv_conf_s ngx_http_dina_module_srv_conf_t; 
struct ngx_http_dina_module_srv_conf_s {
    ngx_str_t zoo_addr;
};

typedef struct ngx_http_dina_module_loc_conf_s ngx_http_dina_module_loc_conf_t;
struct ngx_http_dina_module_loc_conf_s {
    ngx_http_upstream_conf_t upstream;
    ngx_http_dina_zoo_config_t zoo_config;
    ngx_http_dina_config_param_t action;
};


#endif
