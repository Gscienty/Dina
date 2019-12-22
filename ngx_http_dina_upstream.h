#ifndef _NGX_HTTP_DINA_UPSTREAM_H
#define _NGX_HTTP_DINA_UPSTREAM_H

#include <ngx_http.h>
#include <ngx_core.h>
#include "ngx_http_dina_conf.h"

ngx_int_t ngx_http_dina_upstream(ngx_http_request_t *r, ngx_int_t (*resolve) (ngx_http_upstream_resolved_t *const, ngx_http_request_t *const));

#endif
