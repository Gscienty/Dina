#include <ngx_config.h>
#include "ngx_http_dina_conf.h"
#include "ngx_http_dina_discovery.h"
#include "ngx_http_dina_expect_resp.h"

static char *ngx_http_dina_service_handler(ngx_conf_t *, ngx_command_t *, void *);
static ngx_int_t ngx_http_dina_handler(ngx_http_request_t *);

static ngx_command_t ngx_http_dina_module_commands[] = {
    {
        ngx_string("dina_zk"),
        NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_dina_module_loc_conf_t, zoo_config.addr),
        NULL
    },
    {
        ngx_string("dina_service"),
        NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
        ngx_http_dina_service_handler,
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

    loc_conf->zoo_config.addr.data = NULL;
    loc_conf->zoo_config.addr.len = 0;
    loc_conf->zoo_config.static_service_name.data = NULL;
    loc_conf->zoo_config.static_service_name.len = 0;

    loc_conf->zoo_config.lengths = NULL;
    loc_conf->zoo_config.values = NULL;

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

static ngx_int_t ngx_http_dina_handler(ngx_http_request_t *r) {
    char cnt[512];
    ngx_str_t discv = { 0, (u_char *) &cnt };
    ngx_str_t service_name = { 0, NULL };
    ngx_http_dina_module_loc_conf_t *lcf = ngx_http_get_module_loc_conf(r, ngx_http_dina_module);

    if (lcf->zoo_config.static_service_name.len != 0) {
        ngx_http_dina_discovery(&discv, &lcf->zoo_config, &lcf->zoo_config.static_service_name);
    }
    else {
        if (ngx_http_script_run(r, &service_name, lcf->zoo_config.lengths->elts, 0, lcf->zoo_config.values->elts) == NULL) {
            return NGX_ERROR;
        }

        if (ngx_http_dina_discovery(&discv, &lcf->zoo_config, &service_name) != 0) {
            return NGX_ERROR;
        }
    }
    if (discv.len == 0) {
        return ngx_http_dina_service_not_found(r);
    }

    ngx_http_finalize_request(r, NGX_DONE);
    return NGX_DONE;
}

static char *ngx_http_dina_service_handler(ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
    (void) cmd;
    ngx_http_script_compile_t sc;
    ngx_http_dina_module_loc_conf_t *lcf = conf;
    ngx_str_t *values = cf->args->elts;
    ngx_str_t *url = &values[1];
    ngx_uint_t n = ngx_http_script_variables_count(url);
    ngx_http_core_loc_conf_t *ccf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);

    if (lcf->zoo_config.addr.len == 0) {
        return NGX_CONF_ERROR;
    }
    ccf->handler = ngx_http_dina_handler;

    if (n) {
        ngx_memzero(&sc, sizeof(ngx_http_script_compile_t));

        sc.cf = cf;
        sc.source = url;
        sc.lengths = &lcf->zoo_config.lengths;
        sc.values = &lcf->zoo_config.values;
        sc.variables = n;
        sc.complete_lengths = 1;
        sc.complete_values = 1;

        if (ngx_http_script_compile(&sc) != NGX_OK) {
            return NGX_CONF_ERROR;
        }

        return NGX_CONF_OK;
    }

    ngx_conf_set_str_slot(cf, cmd, &lcf->zoo_config.static_service_name);

    return NGX_CONF_OK;
}
