#include "ngx_http_dina_upstream.h"
#include "ngx_http_dina_conf.h"

extern ngx_module_t ngx_http_dina_module;

static char ngx_http_dina_upstream_http_version[] = "HTTP/1.0" CRLF;

static ngx_int_t ngx_http_dina_upstream_create_request(ngx_http_request_t *);
static void ngx_http_dina_upstream_abort_request(ngx_http_request_t *);
static void ngx_http_dina_upstream_finalize_request(ngx_http_request_t *, ngx_int_t);
static ngx_int_t ngx_http_dina_upstream_process_status_line(ngx_http_request_t *);
static ngx_int_t ngx_http_dina_upstream_process_header(ngx_http_request_t *);
static ngx_int_t ngx_http_dina_upstream_copy_filter(ngx_event_pipe_t *, ngx_buf_t *);
static ngx_int_t ngx_http_dina_upstream_input_filter_init(void *);
static ngx_int_t ngx_http_dina_upstream_chunked_filter(ngx_event_pipe_t *, ngx_buf_t *);
static ngx_int_t ngx_http_dina_upstream_non_buffered_chunked_filter(void *, ssize_t);

ngx_int_t ngx_http_dina_upstream(ngx_http_request_t *r,
                                 const ngx_str_t *const action,
                                 ngx_int_t (*resolve) (ngx_http_upstream_resolved_t *const, ngx_http_request_t *const)) {
    ngx_http_upstream_t *u;
    ngx_http_dina_module_loc_conf_t *lcf = ngx_http_get_module_loc_conf(r, ngx_http_dina_module);
    
    if (ngx_http_upstream_create(r) != NGX_OK) {
        return NGX_ERROR;
    }

    u = r->upstream;
    u->conf = &lcf->upstream;
    u->output.tag = (ngx_buf_tag_t) &ngx_http_dina_module;

    u->create_request = ngx_http_dina_upstream_create_request;
    u->abort_request = ngx_http_dina_upstream_abort_request;
    u->process_header = ngx_http_dina_upstream_process_status_line;
    u->finalize_request = ngx_http_dina_upstream_finalize_request;

    if ((u->resolved = ngx_palloc(r->pool, sizeof(ngx_http_upstream_resolved_t))) == NULL) {
        return NGX_ERROR;
    }
    if (resolve(u->resolved, r) != NGX_OK) {
        return NGX_ERROR;
    }

    u->buffering = 0;

    if ((u->pipe = ngx_palloc(r->pool, sizeof(ngx_event_pipe_t))) == NULL) {
        return NGX_ERROR;
    }
    u->pipe->input_filter = ngx_http_dina_upstream_copy_filter;
    u->pipe->input_ctx = r;

    u->input_filter_init = ngx_http_dina_upstream_input_filter_init;
    u->input_filter = ngx_http_dina_upstream_non_buffered_chunked_filter;
    u->input_filter_ctx = r;

    r->main->count++;

    ngx_http_set_ctx(r, (void *) action, ngx_http_dina_module);

    return ngx_http_read_client_request_body(r, ngx_http_upstream_init);
}

static ngx_int_t ngx_http_dina_upstream_create_request(ngx_http_request_t *r) {
    ngx_http_upstream_t *u = r->upstream;
    ngx_list_part_t *part;
    ngx_table_elt_t *header = NULL;
    ngx_chain_t *cl;
    ngx_chain_t *body;
    ngx_buf_t *b;
    ngx_str_t method;
    size_t len = 0;
    ngx_uint_t idx;
    const ngx_str_t *action = ngx_http_get_module_ctx(r, ngx_http_dina_module);

    if (u->method.len) {
        method = u->method;
    }
    else {
        method = r->method_name;
    }

    len += method.len + 1
        + ((action != NULL && action->len != 0) ? action->len : r->uri.len) + 1
        + sizeof(ngx_http_dina_upstream_http_version)
        + sizeof(CRLF) - 1;

    part = &r->headers_in.headers.part;
    header = part->elts;
    for (idx = 0; ; idx++) {
        if (idx >= part->nelts) {
            if (part->next == NULL) {
                break;
            }
            part = part->next;
            header = part->elts;
            idx = 0;
        }
        len += header[idx].key.len + sizeof(": ") - 1
            + header[idx].value.len + sizeof(CRLF) - 1;
    }

    if ((b = ngx_create_temp_buf(r->pool, len)) == NULL) {
        return NGX_ERROR;
    }

    if ((cl = ngx_alloc_chain_link(r->pool)) == NULL) {
        return NGX_ERROR;
    }

    cl->buf = b;

    b->last = ngx_copy(b->last, method.data, method.len);
    *b->last++ = ' ';
    if (action != NULL && action->len != 0) {
        b->last = ngx_copy(b->last, action->data, action->len);
    }
    else {
        b->last = ngx_copy(b->last, r->uri.data, r->uri.len);
    }
    *b->last++ = ' ';
    b->last = ngx_copy(b->last, ngx_http_dina_upstream_http_version, sizeof(ngx_http_dina_upstream_http_version) - 1);

    part = &r->headers_in.headers.part;
    header = part->elts;
    for (idx = 0; ; idx++) {
        if (idx >= part->nelts) {
            if (part->next == NULL) {
                break;
            }
            part = part->next;
            header = part->elts;
            idx = 0;
        }

        b->last = ngx_copy(b->last, header[idx].key.data, header[idx].key.len);
        *b->last++ = ':';
        *b->last++ = ' ';
        b->last = ngx_copy(b->last, header[idx].value.data, header[idx].value.len);
        *b->last++ = CR;
        *b->last++ = LF;
    }
    *b->last++ = CR;
    *b->last++ = LF;

    if (r->request_body_no_buffering) {
        u->request_bufs = cl;
    }
    else {
        body = u->request_bufs;
        u->request_bufs = cl;

        while (body) {
            if ((b = ngx_alloc_buf(r->pool)) == NULL) {
                return NGX_ERROR;
            }

            ngx_memcpy(b, body->buf, sizeof(ngx_buf_t));

            if ((cl->next = ngx_alloc_chain_link(r->pool)) == NULL) {
                return NGX_ERROR;
            }

            cl = cl->next;
            cl->buf = b;

            body = body->next;
        }
    }

    b->flush = 1;
    cl->next = NULL;

    return NGX_OK;
}

static void ngx_http_dina_upstream_abort_request(ngx_http_request_t * r) {
    (void) r;
}

static void ngx_http_dina_upstream_finalize_request(ngx_http_request_t * r, ngx_int_t ret) {
    (void) r;
    (void) ret;
}

static ngx_int_t ngx_http_dina_upstream_process_status_line(ngx_http_request_t * r) {
    size_t len;
    ngx_int_t ret;
    ngx_http_status_t status;
    ngx_http_upstream_t *u = r->upstream;

    if ((ret = ngx_http_parse_status_line(r, &u->buffer, &status)) == NGX_AGAIN) {
        return ret;
    }
    if (ret == NGX_ERROR) {
        r->http_version = NGX_HTTP_VERSION_9;
        u->state->status = NGX_HTTP_OK;
        u->headers_in.connection_close = 1;

        return NGX_OK;
    }

    if (u->state && u->state->status == 0) {
        u->state->status = status.code;
    }

    u->headers_in.status_n = status.code;
    len = status.end - status.start;
    u->headers_in.status_line.len = len;
    if ((u->headers_in.status_line.data = ngx_palloc(r->pool, len)) == NULL) {
        return NGX_ERROR;
    }
    ngx_memcpy(u->headers_in.status_line.data, status.start, len);

    u->headers_in.connection_close = 1;

    u->process_header = ngx_http_dina_upstream_process_header;
    return ngx_http_dina_upstream_process_header(r);
}

static ngx_int_t ngx_http_dina_upstream_process_header(ngx_http_request_t *r) {
    ngx_int_t ret;
    ngx_table_elt_t *h;

    for (;;) {
        ret = ngx_http_parse_header_line(r, &r->upstream->buffer, 1);

        switch (ret) {
        case NGX_OK:
            if ((h = ngx_list_push(&r->upstream->headers_in.headers)) == NULL) {
                return NGX_ERROR;
            }
            h->hash = r->header_hash;
            h->key.len = r->header_name_end - r->header_name_start;
            h->value.len = r->header_end - r->header_start;
            
            if ((h->key.data = ngx_palloc(r->pool, h->key.len + 1 + h->value.len + 1 + h->key.len)) == NULL) {
                h->hash = 0;
                return NGX_ERROR;
            }
            h->value.data = h->key.data + h->key.len + 1;
            h->lowcase_key = h->key.data + h->key.len + 1 + h->value.len + 1;

            ngx_memcpy(h->key.data, r->header_name_start, h->key.len);
            h->key.data[h->key.len] = 0;
            ngx_memcpy(h->value.data, r->header_start, h->value.len);
            h->value.data[h->value.len] = 0;
            if (h->key.len == r->lowcase_index) {
                ngx_memcpy(h->lowcase_key, r->lowcase_header, h->key.len);
            }
            else {
                ngx_strlow(h->lowcase_key, h->key.data, h->key.len);
            }
            break;
        case NGX_HTTP_PARSE_HEADER_DONE:
            return NGX_OK;
        case NGX_AGAIN:
            return NGX_AGAIN;
        default:
            return NGX_HTTP_UPSTREAM_INVALID_HEADER;
        }
    }
}

static ngx_int_t ngx_http_dina_upstream_copy_filter(ngx_event_pipe_t *p, ngx_buf_t *buf) {
    ngx_buf_t *b;
    ngx_chain_t *cl;
    ngx_http_request_t *r;

    if (buf->pos == buf->last) {
        return NGX_OK;
    }
    
    if ((cl = ngx_chain_get_free_buf(p->pool, &p->free)) == NULL) {
        return NGX_ERROR;
    }

    b = cl->buf;

    ngx_memcpy(b, buf, sizeof(ngx_buf_t));
    b->shadow = buf;
    b->tag = p->tag;
    b->last_shadow = 1;
    b->recycled = 1;
    buf->shadow = b;

    if (p->in) {
        *p->last_in = cl;
    }
    else {
        p->in = cl;
    }
    p->last_in = &cl->next;

    if (p->length == -1) {
        return NGX_OK;
    }

    p->length -= b->last - b->pos;

    if (p->length == 0) {
        r = p->input_ctx;
        p->upstream_done = 1;
        r->upstream->keepalive = !r->upstream->headers_in.connection_close;
    }
    else if (p->length < 0) {
        r = p->input_ctx;
        p->upstream_done = 1;
    }

    return NGX_OK;
}

static ngx_int_t ngx_http_dina_upstream_input_filter_init(void *data) {
    ngx_http_request_t *r = data;
    ngx_http_upstream_t *u = r->upstream;

    if (u->headers_in.status_n == NGX_HTTP_NO_CONTENT
        || u->headers_in.status_n == NGX_HTTP_NOT_MODIFIED) {
        u->pipe->length = 0;
        u->length = 0;
        u->keepalive = !u->headers_in.connection_close;
    }
    else if (u->headers_in.chunked) {
        u->pipe->input_filter = ngx_http_dina_upstream_chunked_filter;
        u->pipe->length = 3;
        u->input_filter = ngx_http_dina_upstream_non_buffered_chunked_filter;
        u->length = 1;
    }
    else if (u->headers_in.content_length_n == 0) {
        u->pipe->length = 0;
        u->length = 0;
        u->keepalive = !u->headers_in.connection_close;
    }
    else {
        u->pipe->length = u->headers_in.content_length_n;
        u->length = u->headers_in.content_length_n;
    }

    return NGX_OK;
}

static ngx_int_t ngx_http_dina_upstream_chunked_filter(ngx_event_pipe_t *p, ngx_buf_t *buf) {
    ngx_int_t ret;
    ngx_buf_t *b;
    ngx_buf_t **prev;
    ngx_chain_t *cl;
    ngx_http_request_t *r;
    ngx_http_chunked_t chunked;

    if (buf->last == buf->pos) {
        return NGX_OK;
    }

    r = p->input_ctx;

    b = NULL;
    prev = &b->shadow;

    for (;;) {
        ret = ngx_http_parse_chunked(r, buf, &chunked);
        switch (ret) {
        case NGX_OK:
            if ((cl = ngx_chain_get_free_buf(p->pool, &p->free)) == NULL) {
                return NGX_ERROR;
            }
            b = cl->buf;
            ngx_memzero(b, sizeof(ngx_buf_t));
            b->pos = buf->pos;
            b->start = buf->start;
            b->end = buf->end;
            b->tag = buf->tag;
            b->temporary = 1;
            b->recycled = 1;

            *prev = b;
            prev = &b->shadow;

            if (p->in) {
                *p->last_in = cl;
            }
            else {
                p->in = cl;
            }
            p->last_in = &cl->next;

            b->num = buf->num;

            if (buf->last - buf->pos >= chunked.size) {
                buf->pos += chunked.size;
                b->last = buf->pos;
                chunked.size = 0;
            }
            else {
                chunked.size -= buf->last - buf->pos;
                buf->pos = buf->last;
                b->last = buf->last;
            }

            break;

        case NGX_DONE:
            p->upstream_done = 1;
            r->upstream->keepalive = !r->upstream->headers_in.connection_close;

            goto out_loop;

        case NGX_AGAIN:
            p->length = chunked.length;
            
            goto out_loop;

        default:
            return NGX_ERROR;
        }
    }

out_loop:
    if (b) {
        b->shadow = buf;
        b->last_shadow = 1;

        return NGX_OK;
    }

    if (ngx_event_pipe_add_free_buf(p, buf) != NGX_OK) {
        return NGX_ERROR;
    }

    return NGX_OK;
}

static ngx_int_t ngx_http_dina_upstream_non_buffered_chunked_filter(void *data, ssize_t bytes) {
    ngx_http_request_t *r = data;
    ngx_buf_t *b;
    ngx_chain_t *cl;
    ngx_chain_t **ll;
    ngx_http_upstream_t *u;

    u = r->upstream;

    for (cl = u->out_bufs, ll = &u->out_bufs; cl; cl = cl->next) {
        ll = &cl->next;
    }

    if ((cl = ngx_chain_get_free_buf(r->pool, &u->free_bufs)) == NULL) {
        return NGX_ERROR;
    }

    *ll = cl;

    cl->buf->flush = 1;
    cl->buf->memory = 1;

    b = &u->buffer;

    cl->buf->pos = b->last;
    b->last += bytes;
    cl->buf->last = b->last;
    cl->buf->tag = u->output.tag;

    if (u->length == -1) {
        return NGX_OK;
    }

    u->length -= bytes;

    if (u->length == 0) {
        u->keepalive = !u->headers_in.connection_close;
    }

    return NGX_OK;
}
