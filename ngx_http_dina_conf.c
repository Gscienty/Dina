#include <ngx_config.h>
#include "ngx_http_dina_conf.h"
#include "ngx_http_dina_discovery.h"
#include "ngx_http_dina_expect_resp.h"
#include "ngx_http_dina_upstream.h"

static char *ngx_http_dina_service_handler(ngx_conf_t *, ngx_command_t *, void *);
static char *ngx_http_dina_action_handler(ngx_conf_t *, ngx_command_t *, void *);
static char *ngx_http_dina_conf_set_param_slot(ngx_conf_t *, ngx_command_t *, ngx_http_dina_config_param_t *);
static ngx_int_t ngx_http_dina_handler(ngx_http_request_t *);
static ngx_int_t ngx_http_dina_resolve(ngx_http_upstream_resolved_t *const, ngx_http_request_t *const);
static void *ngx_http_dina_module_create_loc_conf(ngx_conf_t *const cf);
static void *ngx_http_dina_module_create_srv_conf(ngx_conf_t *const cf);

static ngx_command_t ngx_http_dina_module_commands[] = {
    {
        ngx_string("dina_zk"),
        NGX_HTTP_SRV_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_SRV_CONF_OFFSET,
        offsetof(ngx_http_dina_module_srv_conf_t, zoo_addr),
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
    {
        ngx_string("dina_action"),
        NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
        ngx_http_dina_action_handler,
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
    ngx_http_dina_module_create_srv_conf,
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

static void *ngx_http_dina_module_create_loc_conf(ngx_conf_t *const cf) {
    ngx_http_dina_module_loc_conf_t *loc_conf = NULL;

    if ((loc_conf = ngx_palloc(cf->pool, sizeof(*loc_conf))) == NULL) {
        return NULL;
    }

    loc_conf->action.lengths = NULL;
    loc_conf->action.values = NULL;
    loc_conf->action.static_param.data = NULL;
    loc_conf->action.static_param.len = 0;

    loc_conf->zoo_config.service.lengths = NULL;
    loc_conf->zoo_config.service.values = NULL;
    loc_conf->zoo_config.service.static_param.data = NULL;
    loc_conf->zoo_config.service.static_param.data = NULL;

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

static void *ngx_http_dina_module_create_srv_conf(ngx_conf_t *const cf) {
    ngx_http_dina_module_srv_conf_t *srv_conf = NULL;
    if ((srv_conf = ngx_palloc(cf->pool, sizeof(*srv_conf))) == NULL) {
        return NULL;
    }
    srv_conf->zoo_addr.data = NULL;
    srv_conf->zoo_addr.len = 0;
    return srv_conf;
}

static ngx_int_t ngx_http_dina_handler(ngx_http_request_t *r) {
    ngx_str_t action = { 0, NULL };
    ngx_http_dina_module_loc_conf_t *lcf = ngx_http_get_module_loc_conf(r, ngx_http_dina_module);

    if (lcf->zoo_config.service.static_param.len == 0 && lcf->zoo_config.service.lengths == NULL) {
        ngx_http_dina_service_not_found(r);
        return NGX_DONE;
    }

    if (lcf->action.static_param.len != 0) {
        action = lcf->action.static_param;
    }
    else if (lcf->action.lengths != NULL) {
        if (ngx_http_script_run(r, &action, lcf->action.lengths->elts, 0, lcf->action.values->elts) == NULL) {
            return NGX_ERROR;
        }
    }

    ngx_http_dina_upstream(r, &action, ngx_http_dina_resolve);
    ngx_http_finalize_request(r, NGX_DONE);
    return NGX_DONE;
}

static char *ngx_http_dina_conf_set_param_slot(ngx_conf_t *cf, ngx_command_t *cmd, ngx_http_dina_config_param_t *param) {
    (void) cmd;
    ngx_http_script_compile_t sc;
    ngx_str_t *values = cf->args->elts;
    ngx_str_t *source = &values[1];
    ngx_uint_t n = ngx_http_script_variables_count(source);
    ngx_http_core_loc_conf_t *ccf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);

    ccf->handler = ngx_http_dina_handler;

    if (n) {
        ngx_memzero(&sc, sizeof(ngx_http_script_compile_t));

        sc.cf = cf;
        sc.source = source;
        sc.lengths = &param->lengths;
        sc.values = &param->values;
        sc.variables = n;
        sc.complete_lengths = 1;
        sc.complete_values = 1;

        if (ngx_http_script_compile(&sc) != NGX_OK) {
            return NGX_CONF_ERROR;
        }

        return NGX_CONF_OK;
    }

    ngx_conf_set_str_slot(cf, cmd, &param->static_param);

    return NGX_CONF_OK;
}

static ngx_int_t ngx_http_dina_resolve(ngx_http_upstream_resolved_t *const resolved, ngx_http_request_t *const r) {
    ngx_str_t *discv;
    ngx_str_t service_name = { 0, NULL };
    ngx_http_dina_module_loc_conf_t *lcf = ngx_http_get_module_loc_conf(r, ngx_http_dina_module);
    ngx_http_dina_module_srv_conf_t *scf = ngx_http_get_module_srv_conf(r, ngx_http_dina_module);
    char *port_split;
    int port;
    struct sockaddr_in *in_addr = NULL;

    if ((discv = ngx_palloc(r->pool, sizeof(ngx_str_t))) == NULL) {
        return NGX_ERROR;
    }
    if ((discv->data = ngx_palloc(r->pool, 64)) == NULL) {
        return NGX_ERROR;
    }
    memset(discv->data, 0, 64);

    if (lcf->zoo_config.service.static_param.len != 0) {
        if (ngx_strchr(lcf->zoo_config.service.static_param.data, '@') != NULL) {
            ngx_http_dina_service_unauthorized(r);
            return NGX_HTTP_UNAUTHORIZED;
        }
        ngx_http_dina_discovery(discv, &scf->zoo_addr, &lcf->zoo_config.service.static_param);
    }
    else {
        if (ngx_http_script_run(r, &service_name, lcf->zoo_config.service.lengths->elts, 0, lcf->zoo_config.service.values->elts) == NULL) {
            return NGX_ERROR;
        }
        if (ngx_strchr(service_name.data, '@') != NULL) {
            ngx_http_dina_service_unauthorized(r);
            return NGX_HTTP_UNAUTHORIZED;
        }
        if (ngx_http_dina_discovery(discv, &scf->zoo_addr, &service_name) != 0) {
            return NGX_ERROR;
        }
    }
    if (discv->len == 0) {
        ngx_http_dina_service_not_found(r);
        return NGX_HTTP_NOT_FOUND;
    }

    port_split = ngx_strchr(discv->data, ':');
    if (port_split == NULL) {
        port = 80;
    }
    else {
        *port_split = 0;
        port = atoi(port_split + 1);
    }

    if ((in_addr = ngx_palloc(r->pool, sizeof(struct sockaddr_in))) == NULL) {
        return NGX_ERROR;
    }
    resolved->sockaddr = (struct sockaddr *) in_addr;
    resolved->socklen = sizeof(struct sockaddr_in);
    resolved->naddrs = 1;
    resolved->port = port;

    in_addr->sin_family = AF_INET;
    in_addr->sin_port = htons(port);
    inet_aton((const char *) discv->data, &in_addr->sin_addr);

    discv->len = strlen((const char *) discv->data);
    ngx_http_set_ctx(r, discv, ngx_http_dina_module);

    return NGX_OK;
}

static char *ngx_http_dina_service_handler(ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
    ngx_http_dina_module_loc_conf_t *lcf = conf;

    return ngx_http_dina_conf_set_param_slot(cf, cmd, &lcf->zoo_config.service);
}

static char *ngx_http_dina_action_handler(ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
    ngx_http_dina_module_loc_conf_t *lcf = conf;
    
    return ngx_http_dina_conf_set_param_slot(cf, cmd, &lcf->action);
}
