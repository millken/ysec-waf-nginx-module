#include "ngx_yy_sec_waf_re.h"

/*
** @description: This function is called to get args.
** @para: ngx_http_request_t *r
** @para: ngx_http_variable_value_t *v
** @para: uintptr_t data
** @return: NGX_OK or NGX_ERROR if failed.
*/

static ngx_int_t
yy_sec_waf_get_args(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    ngx_http_request_ctx_t    *ctx;
    ngx_str_t                  p;

    ctx = ngx_http_get_module_ctx(r, ngx_http_yy_sec_waf_module);

    if (ctx == NULL) {
        v->not_found = 1;
        return NGX_OK;
    }

    if (ctx->args.len == 0 && ctx->post_args.len == 0){
        v->not_found = 1;
        return NGX_OK;
    }

    v->data = ctx->args.data;
    v->len = ctx->args.len;

    if (ctx->post_args.len) {
        p.len = ctx->args.len+ctx->post_args.len+1;
        p.data = ngx_palloc(r->pool, p.len);
        if (p.data == NULL) {
            return NGX_ERROR;
        }

        v->data = p.data;
        v->len = p.len;

        p.data = ngx_cpymem(p.data, ctx->args.data, ctx->args.len);
        p.data = ngx_cpymem(p.data, ",", 1);
        ngx_memcpy(p.data, ctx->post_args.data, ctx->post_args.len);
    }

    if (r->method == NGX_HTTP_POST) {
        
        ctx->raw_string = ctx->full_body;
    } else if (r->method == NGX_HTTP_GET) {
    
        ctx->raw_string = &r->args;
    }

    v->valid = 1;
    v->no_cacheable = 0;
    v->escape = 0;
    v->not_found = 0;

    return NGX_OK;
}

/*
** @description: This function is called to get post args count.
** @para: ngx_http_request_t *r
** @para: ngx_http_variable_value_t *v
** @para: uintptr_t data
** @return: NGX_OK or NGX_ERROR if failed.
*/

static ngx_int_t
yy_sec_waf_get_post_args_count(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    u_char                    *p;
    ngx_http_request_ctx_t    *ctx;

    ctx = ngx_http_get_module_ctx(r, ngx_http_yy_sec_waf_module);

    if (ctx == NULL) {
        v->not_found = 1;
        return NGX_OK;
    }

    if (ctx->post_args_count == 0) {
        v->not_found = 1;
        return NGX_OK;
    }

    p = ngx_yy_sec_waf_uitoa(r->pool, ctx->post_args_count);

    v->len = ngx_strlen(p);
    v->valid = 1;
    v->no_cacheable = 0;
    v->escape = 0;
    v->not_found = 0;
    v->data = p;

    return NGX_OK;
}

/*
** @description: This function is called to get process body error.
** @para: ngx_http_request_t *r
** @para: ngx_http_variable_value_t *v
** @para: uintptr_t data
** @return: NGX_OK or NGX_ERROR if failed.
*/

static ngx_int_t
yy_sec_waf_get_process_body_error(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    ngx_http_request_ctx_t    *ctx;

    ctx = ngx_http_get_module_ctx(r, ngx_http_yy_sec_waf_module);

    if (ctx == NULL) {
        v->not_found = 1;
        return NGX_OK;
    }

    if (ctx->process_body_error == 1) {
        *v = ngx_http_variable_true_value;
    } else {
        v->not_found = 1;
    }

    return NGX_OK;
}

/*
** @description: This function is called to get multipart name.
** @para: ngx_http_request_t *r
** @para: ngx_http_variable_value_t *v
** @para: uintptr_t data
** @return: NGX_OK or NGX_ERROR if failed.
*/

static ngx_int_t
yy_sec_waf_get_multipart_name(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    ngx_uint_t                 i;
    ngx_str_t                 *var;
    u_char                    *p;
    ngx_http_request_ctx_t    *ctx;

    ctx = ngx_http_get_module_ctx(r, ngx_http_yy_sec_waf_module);

    if (ctx == NULL) {
        v->not_found = 1;
        return NGX_OK;
    }

    var = ctx->multipart_name.elts;

    for (i = 0; i < ctx->multipart_name.nelts; i++) {
        v->len += var[i].len;
    }

    if (v->len == 0) {
        v->not_found = 1;
        return NGX_OK;
    }

    v->data = ngx_palloc(r->pool, v->len);

    p = v->data;

    for (i = 0; i < ctx->multipart_name.nelts; i++) {
        v->data = ngx_cpymem(v->data, var[i].data, var[i].len);
    }

    v->valid = 1;
    v->no_cacheable = 0;
    v->escape = 0;
    v->not_found = 0;
    v->data = p;

    return NGX_OK;
}

/*
** @description: This function is called to get multipart filename.
** @para: ngx_http_request_t *r
** @para: ngx_http_variable_value_t *v
** @para: uintptr_t data
** @return: NGX_OK or NGX_ERROR if failed.
*/

static ngx_int_t
yy_sec_waf_get_multipart_filename(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    ngx_uint_t                 i;
    ngx_str_t                 *var;
    u_char                    *p;
    ngx_http_request_ctx_t    *ctx;

    ctx = ngx_http_get_module_ctx(r, ngx_http_yy_sec_waf_module);

    if (ctx == NULL) {
        v->not_found = 1;
        return NGX_OK;
    }

    var = ctx->multipart_filename.elts;

    for (i = 0; i < ctx->multipart_filename.nelts; i++) {
        v->len += var[i].len;
    }

    if (v->len == 0) {
        v->not_found = 1;
        return NGX_OK;
    }

    v->data = ngx_palloc(r->pool, v->len);

    p = v->data;

    for (i = 0; i < ctx->multipart_filename.nelts; i++) {
        v->data = ngx_cpymem(v->data, var[i].data, var[i].len);
    }

    v->valid = 1;
    v->no_cacheable = 0;
    v->escape = 0;
    v->not_found = 0;
    v->data = p;

    return NGX_OK;
}

/*
** @description: This function is called to get multipart contenttype.
** @para: ngx_http_request_t *r
** @para: ngx_http_variable_value_t *v
** @para: uintptr_t data
** @return: NGX_OK or NGX_ERROR if failed.
*/

static ngx_int_t
yy_sec_waf_get_multipart_content_type(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    ngx_uint_t                 i;
    ngx_str_t                 *var;
    u_char                    *p;
    ngx_http_request_ctx_t    *ctx;

    ctx = ngx_http_get_module_ctx(r, ngx_http_yy_sec_waf_module);

    if (ctx == NULL) {
        v->not_found = 1;
        return NGX_OK;
    }

    var = ctx->content_type.elts;

    for (i = 0; i < ctx->content_type.nelts; i++) {
        v->len += var[i].len;
    }

    if (v->len == 0) {
        v->not_found = 1;
        return NGX_OK;
    }

    v->data = ngx_palloc(r->pool, v->len);

    p = v->data;

    for (i = 0; i < ctx->content_type.nelts; i++) {
        v->data = ngx_cpymem(v->data, var[i].data, var[i].len);
    }

    v->valid = 1;
    v->no_cacheable = 0;
    v->escape = 0;
    v->not_found = 0;
    v->data = p;

    return NGX_OK;
}

/*
** @description: This function is called to get connection per ip.
** @para: ngx_http_request_t *r
** @para: ngx_http_variable_value_t *v
** @para: uintptr_t data
** @return: static ngx_int_t.
*/

static ngx_int_t
yy_sec_waf_get_conn_per_ip(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    u_char                    *p;
    ngx_http_request_ctx_t    *ctx;

    ctx = ngx_http_get_module_ctx(r, ngx_http_yy_sec_waf_module);

    if (ctx == NULL) {
        v->not_found = 1;
        return NGX_OK;
    }

    if (ctx->conn_per_ip == 0) {
        v->not_found = 1;
        return NGX_OK;
    }

    p = ngx_yy_sec_waf_uitoa(r->pool, ctx->conn_per_ip);

    v->len = ngx_strlen(p);
    v->valid = 1;
    v->no_cacheable = 0;
    v->escape = 0;
    v->not_found = 0;
    v->data = p;

    return NGX_OK;
}

static ngx_http_variable_t var_metadata[] = {

    { ngx_string("ARGS"), NULL, yy_sec_waf_get_args,
      0, 0, 0 },

    { ngx_string("ARGS_POST"), NULL, yy_sec_waf_get_args,
      0, 0, 0 },

    { ngx_string("POST_ARGS_COUNT"), NULL, yy_sec_waf_get_post_args_count,
      0, 0, 0 },

    { ngx_string("PROCESS_BODY_ERROR"), NULL, yy_sec_waf_get_process_body_error,
      0, 0, 0 },

    { ngx_string("MULTIPART_NAME"), NULL, yy_sec_waf_get_multipart_name,
      0, 0, 0 },

    { ngx_string("MULTIPART_FILENAME"), NULL, yy_sec_waf_get_multipart_filename,
      0, 0, 0 },

    { ngx_string("MULTIPART_CONTENT_TYPE"), NULL, yy_sec_waf_get_multipart_content_type,
      0, 0, 0 },

    { ngx_string("CONN_PER_IP"), NULL, yy_sec_waf_get_conn_per_ip,
      0, 0, 0 },

    { ngx_null_string, NULL, NULL,
      0, 0, 0 }
};

ngx_int_t
ngx_http_yy_sec_waf_add_variables(ngx_conf_t *cf)
{
    ngx_http_variable_t *var, *v;

    for (v = var_metadata; v->name.len != 0; v++) {
        var = ngx_http_add_variable(cf, &v->name, v->flags);
        if (var == NULL) {
            return NGX_ERROR;
        }

        var->get_handler = v->get_handler;
        var->flags = v->flags;
    }
    
    return NGX_OK;
}


