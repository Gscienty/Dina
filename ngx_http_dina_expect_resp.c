#include "ngx_http_dina_expect_resp.h"

int ngx_http_dina_service_not_found(ngx_http_request_t *const r) {
    ngx_str_t type = ngx_string("application/json");
    ngx_str_t msg = ngx_string("{\"dina_err\": \"not_found\"}");
    ngx_http_discard_request_body(r);

    r->headers_out.status = NGX_HTTP_NOT_FOUND;
    r->headers_out.content_length_n = msg.len;
    r->headers_out.content_type = type;
    ngx_http_send_header(r);

    ngx_buf_t *b = ngx_create_temp_buf(r->pool, msg.len);
    ngx_memcpy(b->pos, msg.data, msg.len);
    b->last = b->pos + msg.len;
    b->last_buf = 1;

    ngx_chain_t out;
    out.buf = b;
    out.next = NULL;

    return ngx_http_output_filter(r, &out);
}

int ngx_http_dina_service_unauthorized(ngx_http_request_t *const r) {
    ngx_str_t type = ngx_string("application/json");
    ngx_str_t msg = ngx_string("{\"dina_err\": \"unauthorized\"}");
    ngx_http_discard_request_body(r);

    r->headers_out.status = NGX_HTTP_UNAUTHORIZED;
    r->headers_out.content_length_n = msg.len;
    r->headers_out.content_type = type;
    ngx_http_send_header(r);

    ngx_buf_t *b = ngx_create_temp_buf(r->pool, msg.len);
    ngx_memcpy(b->pos, msg.data, msg.len);
    b->last = b->pos + msg.len;
    b->last_buf = 1;

    ngx_chain_t out;
    out.buf = b;
    out.next = NULL;

    return ngx_http_output_filter(r, &out);
}

