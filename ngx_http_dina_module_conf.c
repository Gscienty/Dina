#include <ngx_config.h>
#include "ngx_http_dina_module_conf.h"

static char *ngx_http_dina_command_handler(ngx_conf_t *, ngx_command_t *, void *);
static ngx_int_t ngx_http_dina_handler(ngx_http_request_t *);

static ngx_command_t ngx_http_dina_module_commands[] = {
    {
        ngx_string("dina_zk"),
        NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_dina_module_loc_conf_t, zk_addr),
        NULL
    },
    {
        ngx_string("dina_run"),
        NGX_HTTP_LOC_CONF | NGX_CONF_NOARGS,
        ngx_http_dina_command_handler,
        NGX_HTTP_LOC_CONF_OFFSET,
        0,
        NULL
    },
    ngx_null_command
};

static ngx_http_module_t ngx_http_dina_module_ctx = {
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    ngx_http_dina_module_create_loc_conf,
    NULL
};

ngx_module_t ngx_http_dina_module = {
    NGX_MODULE_V1,
    &ngx_http_dina_module_ctx,
    ngx_http_dina_module_commands,
    NGX_HTTP_MODULE,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NGX_MODULE_V1_PADDING
};

void *ngx_http_dina_module_create_loc_conf(ngx_conf_t *const cf) {
    ngx_http_dina_module_loc_conf_t *loc_conf = NULL;

    if ((loc_conf = ngx_palloc(cf->pool, sizeof(*loc_conf))) == NULL) {
        return NULL;
    }

    loc_conf->zk_addr.data = NULL;
    loc_conf->zk_addr.len = 0;

    loc_conf->upstream.connect_timeout = 60000;
    loc_conf->upstream.send_timeout = 60000;
    loc_conf->upstream.read_timeout = 60000;
    loc_conf->upstream.store_access = 0600;

    loc_conf->upstream.buffering = 0;
    loc_conf->upstream.bufs.num = 8;
    loc_conf->upstream.bufs.size = ngx_pagesize;
    loc_conf->upstream.buffer_size = ngx_pagesize;
    loc_conf->upstream.busy_buffers_size = ngx_pagesize << 1;

    loc_conf->upstream.max_temp_file_size = 1024 * 1024 * 1024;
    loc_conf->upstream.temp_file_write_size = ngx_pagesize << 1;

    loc_conf->upstream.hide_headers = NGX_CONF_UNSET_PTR;
    loc_conf->upstream.pass_headers = NGX_CONF_UNSET_PTR;

    return loc_conf;
}

static char *ngx_http_dina_command_handler(ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
    (void) cmd;
    (void) conf;
    ngx_http_core_loc_conf_t *ccf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);

    ccf->handler = ngx_http_dina_handler;
    
    return NGX_CONF_OK;
}

static ngx_int_t ngx_http_dina_handler(ngx_http_request_t *r) {
    ngx_http_dina_module_loc_conf_t *lcf = ngx_http_get_module_loc_conf(r, ngx_http_dina_module);

    return NGX_DONE;
}
