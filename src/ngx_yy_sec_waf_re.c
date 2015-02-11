#include "ngx_yy_sec_waf_re.h"

static yy_sec_waf_re_t *rule_engine;

static ngx_str_t yy_sec_waf_content_type = ngx_string("text/html");

extern ngx_int_t ngx_local_addr(const char *eth, ngx_str_t *s);

static ngx_int_t
yy_sec_waf_re_process_block_list(ngx_http_request_t *r,
    ngx_array_t *block_list, ngx_http_request_ctx_t *ctx);

/*
** @description: This function is called to resolve tfns in hash.
** @para: ngx_str_t *action
** @return: static re_action_metadata *
*/

re_tfns_metadata *
yy_sec_waf_re_resolve_tfn_in_hash(ngx_str_t *tfn)
{
    ngx_uint_t        key;
    re_tfns_metadata *metadata;

    if (tfn == NULL) {
        return NULL;
    }

    key = ngx_hash_key_lc(tfn->data, tfn->len);
    ngx_strlow(tfn->data, tfn->data, tfn->len);

    metadata = (re_tfns_metadata *)ngx_hash_find(
        &rule_engine->tfns_in_hash, key, tfn->data, tfn->len);

    return metadata;
}

/*
** @description: This function is called to resolve operators in hash.
** @para: ngx_str_t *operator
** @return: static re_op_metadata *
*/

static re_op_metadata *
yy_sec_waf_re_resolve_operator_in_hash(ngx_str_t *operator)
{
    ngx_uint_t      key;
    re_op_metadata *metadata;

    if (operator == NULL) {
        return NULL;
    }

    key = ngx_hash_key_lc(operator->data, operator->len);
    ngx_strlow(operator->data, operator->data, operator->len);

    metadata = (re_op_metadata *)ngx_hash_find(
        &rule_engine->operators_in_hash, key, operator->data, operator->len);

    return metadata;
}

/*
** @description: This function is called to resolve actions in hash.
** @para: ngx_str_t *action
** @return: static re_action_metadata *
*/

re_action_metadata *
yy_sec_waf_re_resolve_action_in_hash(ngx_str_t *action)
{
    ngx_uint_t          key;
    re_action_metadata *metadata;

    if (action == NULL) {
        return NULL;
    }

    key = ngx_hash_key_lc(action->data, action->len);
    ngx_strlow(action->data, action->data, action->len);

    metadata = (re_action_metadata *)ngx_hash_find(
        &rule_engine->actions_in_hash, key, action->data, action->len);

    return metadata;
}

/*
** @description: This function is called to redirect request url to the denied url of yy sec waf.
** @para: ngx_http_request_t *r
** @para: ngx_http_request_ctx_t *ctx
** @return: NGX_HTTP_OK or NGX_ERROR if failed.
*/

static ngx_int_t
yy_sec_waf_output_forbidden_page(ngx_http_request_t *r,
    ngx_http_request_ctx_t *ctx)
{
    ngx_http_yy_sec_waf_loc_conf_t *cf;
    ngx_http_complex_value_t        cv;
    int                             status;

    cf = ngx_http_get_module_loc_conf(r, ngx_http_yy_sec_waf_module);

    status = ctx->status? ctx->status: NGX_HTTP_PRECONDITION_FAILED;

    if (cf->denied_url.len != 0) {

        ngx_memzero(&cv, sizeof(ngx_http_complex_value_t));

        cv.value = cf->denied_url;

        ngx_http_send_response(r, status, &yy_sec_waf_content_type, &cv);

        /* we have to finalize the request by ourselves,
               * that is because we use the "read client body" API */
        //ngx_http_finalize_request(r, status);

        return status;
    }

    return status;
}

/*
** @description: This function is called to execute operator.
** @para: ngx_http_request_t *r
** @para: ngx_str_t *str
** @para: ngx_http_yy_sec_waf_rule_t *rule
** @para: ngx_http_request_ctx_t *ctx
** @return: RULE_MATCH or RULE_NO_MATCH if failed.
*/

static ngx_int_t
yy_sec_waf_re_execute_operator(ngx_http_request_t *r,
    ngx_http_yy_sec_waf_rule_t *rule, ngx_http_request_ctx_t *ctx)
{
    ngx_int_t        rc;

    if (rule->op_metadata == NULL) {
        return NGX_ERROR;
    }

    rc = ((re_op_metadata*)rule->op_metadata)->execute(r, &ctx->var, rule);

    if ((rc == RULE_MATCH && !rule->op_negative)
        || (rc == RULE_NO_MATCH && rule->op_negative)) {
        ctx->matched = 1;
        ctx->rule_id = rule->rule_id;
        ctx->action_level = rule->action_level;
        ctx->gids = rule->gids;
        ctx->msg = rule->msg;
        ctx->status = rule->status;
        yy_sec_waf_re_process_block_list(r, ctx->cf->block_list, ctx);

        return RULE_MATCH;
    }

    return RULE_NO_MATCH;
}

/*
** @description: This function is called to perform interception.
** @para: ngx_http_request_ctx_t *ctx
** @return: static ngx_int_t.
*/

static ngx_int_t
yy_sec_waf_re_perform_interception(ngx_http_request_ctx_t *ctx)
{

    if (ctx == NULL 
        || ctx->raw_string == NULL
        || ctx->real_client_ip == NULL
        || ctx->server_ip == NULL) {
        return NGX_DECLINED;
    }

    ngx_atomic_fetch_add(request_matched, 1);

    ctx->process_done = 1;

    if (ctx->action_level & ACTION_LOG)
        ngx_atomic_fetch_add(request_logged, 1);

    if (ctx->action_level & ACTION_ALLOW)
        ngx_atomic_fetch_add(request_allowed, 1);
    
    if (ctx->action_level & ACTION_BLOCK)
        ngx_atomic_fetch_add(request_blocked, 1);

    size_t  len = ctx->raw_string->len;
    u_char *p = ctx->raw_string->data;

    if (len > NGX_MAX_ERROR_STR - 300) {
        len = NGX_MAX_ERROR_STR - 300;
        p[len++] = '.'; p[len++] = '.'; p[len++] = '.';
    }

    if (ctx->action_level & ACTION_LOG) {
        ngx_log_error(NGX_LOG_ERR, ctx->r->connection->log, 0,
            "[ysec_waf] %s, id: %d,"
            " var: %*s, client_ip: %V, server_ip: %V",
            (ctx->action_level & ACTION_BLOCK)? "block":
            (ctx->action_level & ACTION_ALLOW)? "allow": "alert",
            ctx->rule_id,
            len, p,
            ctx->real_client_ip, ctx->server_ip);
    }

    if (ctx->action_level & ACTION_BLOCK)
        return yy_sec_waf_output_forbidden_page(ctx->r, ctx);

    return NGX_DECLINED;
}

/*
** @description: This function is called to process block list for yy sec waf.
** @para: ngx_http_request_t *r
** @para: ngx_http_yy_sec_waf_block_list_t *block_list
** @para: ngx_http_request_ctx_t *ctx
** @return: RULE_MATCH or RULE_NO_MATCH if failed.
*/

static ngx_int_t
yy_sec_waf_re_process_block_list(ngx_http_request_t *r,
    ngx_array_t *block_list, ngx_http_request_ctx_t *ctx)
{
    ngx_uint_t                        i;
    ngx_int_t                         rc;
    ngx_str_t                         str;
    ngx_http_variable_value_t        *vv;
    ngx_http_yy_sec_waf_block_list_t *block_list_p;

	if (block_list == NULL || block_list->elts == NULL)
		return NGX_ERROR;

    block_list_p = (ngx_http_yy_sec_waf_block_list_t*)block_list->elts;
    for (i = 0; i < block_list->nelts; ++i) {
        vv = ngx_http_get_flushed_variable(r, block_list_p[i].idx);
    
        if (vv == NULL || vv->not_found || vv->len == 0) {
            return NGX_AGAIN;
        }
    
        if (ctx->action_level & ACTION_ALLOW) {
            continue;
        }

        str.len = vv->len;
        str.data = vv->data;
    
        ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "[ysec_waf] str:%V", &str);
    
        if (block_list_p[i].regex != NULL) {
            /* REGEX */
            rc = ngx_http_regex_exec(r, block_list_p[i].regex, &str);
        
            if (rc == NGX_OK) {
                ctx->action_level &= ~ACTION_ALLOW;
                ctx->action_level |= ACTION_BLOCK;
                ctx->action_level |= ACTION_LOG;
                return NGX_OK;
            }
        }
    }

    return NGX_ERROR;
}

/*
** @description: This function is called to process rule for yy sec waf.
** @para: ngx_http_request_t *r
** @para: ngx_http_yy_sec_waf_loc_conf_t *cf
** @para: ngx_http_request_ctx_t *ctx
** @return: RULE_MATCH or RULE_NO_MATCH if failed.
*/

static ngx_int_t
yy_sec_waf_re_process_rule(ngx_http_request_t *r,
    ngx_http_yy_sec_waf_rule_t *rule, ngx_http_request_ctx_t *ctx)
{
    ngx_int_t                   rc, *var_index_p;
    ngx_uint_t                  i;
    ngx_http_variable_value_t  *vv;

	if (rule == NULL)
		return NGX_AGAIN;

    var_index_p = rule->var_index.elts;

    for (i = 0; i < rule->var_index.nelts; i++) {

        vv = ngx_http_get_flushed_variable(r, var_index_p[i]);
    
        if (vv == NULL || vv->not_found || vv->len == 0) {
            return NGX_AGAIN;
        }

        ctx->var.data = vv->data;
        ctx->var.len = vv->len;

        ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "[ysec_waf] id:%d, var:%V", rule->rule_id, &ctx->var);

        rc = yy_sec_waf_re_execute_operator(r, rule, ctx);
        if (rc == NGX_ERROR || rc == RULE_MATCH) {
            return rc;
        }
    }

    return RULE_NO_MATCH;
}

/*
** @description: This function is called to process normal rules for yy sec waf.
** @para: ngx_http_request_t *r
** @para: ngx_http_yy_sec_waf_loc_conf_t *cf
** @para: ngx_http_request_ctx_t *ctx
** @return: RULE_MATCH or RULE_NO_MATCH if failed.
*/

ngx_int_t
yy_sec_waf_re_process_normal_rules(ngx_http_request_t *r,
    ngx_http_yy_sec_waf_loc_conf_t *cf, ngx_http_request_ctx_t *ctx, ngx_uint_t phase)
{
    ngx_uint_t                  i, rule_num;
    ngx_int_t                   rc, mode;
    ngx_array_t                *rule_array;
    ngx_http_yy_sec_waf_rule_t *rule;

	if (ctx->cf == NULL) {
		return NGX_ERROR;
	}

    switch(phase) {
        case REQUEST_HEADER_PHASE:
            rule_array = ctx->cf->request_header_rules;
            break;
        case REQUEST_BODY_PHASE:
            rule_array = ctx->cf->request_body_rules;
            break;
        case RESPONSE_HEADER_PHASE:
            rule_array = ctx->cf->response_header_rules;
            break;
        case RESPONSE_BODY_PHASE:
            rule_array = ctx->cf->response_body_rules;
            break;
        default:
            return NGX_ERROR;
    }

    if (rule_array == NULL) {
        return NGX_DECLINED;
    }

	/* If we are here that means the mode is NEXT_RULE, which
	** then means we have done processing any chains.
	*/
    mode = NEXT_RULE;

    rule = rule_array->elts;
    rule_num = rule_array->nelts;

    ctx->phase = phase;

    ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
        "[ysec_waf] phase: %d, rule_num: %d", phase, rule_num);

    for (i=0; i < rule_num; i++) {

		/* NEXT_CHAIN is used when one of the rules in a chain
		**  fails to match and then we need to skip the remaining
		**  rules in that chain in order to get to the next
		**  rule that can execute.
		*/

        if (mode == NEXT_CHAIN) {
            if (rule[i].is_chain == 0) {
                mode = NEXT_RULE;
            }

            continue;
        }

        rc = yy_sec_waf_re_process_rule(r, &rule[i], ctx);

        if (rc == NGX_ERROR) {

            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "[ysec_waf] failed to execute operator");
            return rc;
        } else if (rc == RULE_MATCH) {

            if (rule[i].is_chain == 1) {
                mode = NEXT_RULE;
                continue;
            }

            goto MATCH;
        } else if (rc == RULE_NO_MATCH || rc == NGX_AGAIN) {
        
            if (rule[i].is_chain == 1) {
				/* If the current rule is part of a chain then
                         ** we need to skip over all the rules in the chain.
                         */
                mode = NEXT_CHAIN;
            } else {
                /* This rule is not part of a chain so we simply
                         ** move to the next rule.
                         */
                mode = NEXT_RULE;
            }

            continue;
        }
    }

    return NGX_DECLINED;

MATCH:
    return yy_sec_waf_re_perform_interception(ctx);
}

/*
** @description: This function is called to parse variables.
** @para: ngx_conf_t *cf
** @para: ngx_str_t *value
** @para: ngx_http_yy_sec_waf_rule_t *rule
** @return: static char *
*/

static char *
yy_sec_waf_re_parse_variables(ngx_conf_t *cf,
    ngx_str_t *value, ngx_http_yy_sec_waf_rule_t *rule)
{
    ngx_str_t variable;
    ngx_int_t len, var_index, *var_index_p;
    u_char   *start, *last, *end;

    if (value == NULL) {
        return NGX_CONF_ERROR;
    }

    ngx_memcpy(&variable, value, sizeof(ngx_str_t));

    ngx_array_init(&rule->var_index, cf->pool, 1, sizeof(ngx_int_t));

    len = variable.len;
    start = variable.data;
    last = variable.data+variable.len;

    while(len > 0 && *start) {

        if (*start == '|' || *start == '$'){
            start++;
            continue;
        }

        end = ngx_strlchr(start, last, '|');
        if (end == NULL) {
            end = last;
        }

        variable.data = start;
        variable.len = end - start;

        var_index = ngx_http_get_variable_index(cf, &variable);
        if (var_index == NGX_ERROR) {
            return NGX_CONF_ERROR;
        }

        var_index_p = ngx_array_push(&rule->var_index);
        if (var_index_p == NULL)
            return NGX_CONF_ERROR;

        *var_index_p = var_index;

        start = end+1;
        len = last - start;
    }

    return NGX_CONF_OK;
}

/*
** @description: This function is called to parse operator.
** @para: ngx_conf_t *cf
** @para: ngx_str_t *value
** @para: ngx_http_yy_sec_waf_rule_t *rule
** @return: static char *
*/

static char *
yy_sec_waf_re_parse_operator(ngx_conf_t *cf,
    ngx_str_t *value, ngx_http_yy_sec_waf_rule_t *rule)
{
    u_char              *pos;
    re_op_metadata      *op_metadata;
    ngx_str_t            operator;

    if (value == NULL) {
        return NGX_CONF_ERROR;
    }

    ngx_memcpy(&operator, value, sizeof(ngx_str_t));

    if (operator.data[0] == '!') {
        rule->op_negative = 1;
        operator.data++;
    }

    pos = ngx_strlchr(operator.data, operator.data+operator.len, ':');
    operator.len = pos-operator.data;

    rule->op_metadata = yy_sec_waf_re_resolve_operator_in_hash(&operator);

    if (rule->op_metadata == NULL) {
        return NGX_CONF_ERROR;
    }

    operator.len = value->len;
    if (operator.data[0] == '!') {
        operator.len--;
    }

    op_metadata = (re_op_metadata*) rule->op_metadata;

    if (op_metadata->parse(cf, &operator, rule) != NGX_CONF_OK) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}

/*
** @description: This function is called to read configuration of yy sec waf.
** @para: ngx_conf_t *cf
** @para: ngx_command_t *cmd
** @para: void *conf
** @return: NGX_CONF_OK or NGX_CONF_ERROR if failed.
*/

char *
ngx_http_yy_sec_waf_re_read_conf(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf)
{
    ngx_http_yy_sec_waf_loc_conf_t  *p = conf;

    ngx_uint_t                  n;
    char                       *rc;
    ngx_str_t                  *value, action;
    ngx_shm_zone_t             *shm_zone;
    re_action_metadata         *action_metadata;
    ngx_http_yy_sec_waf_rule_t *rule_p, rule;

    value = cf->args->elts;
    ngx_memset(&rule, 0, sizeof(ngx_http_yy_sec_waf_rule_t));

    /* variable */
    rc = yy_sec_waf_re_parse_variables(cf, &value[1], &rule);
    if (rc == NGX_CONF_ERROR) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "[ysec_waf] yy_sec_waf_re_parse_variables failed");
        return NGX_CONF_ERROR;
    }

    /* operator */
    rc = yy_sec_waf_re_parse_operator(cf, &value[2], &rule);
    if (rc == NGX_CONF_ERROR) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "[ysec_waf] yy_sec_waf_re_parse_operator failed");
        return NGX_CONF_ERROR;
    }

    /* action */
    for (n = 3; n < cf->args->nelts; n++) {
        ngx_memcpy(&action, &value[n], sizeof(ngx_str_t));
        u_char *pos = ngx_strlchr(action.data, action.data+action.len, ':');

        if (pos) {
            action.len = pos-action.data;
        }

        rule.action_metadata = yy_sec_waf_re_resolve_action_in_hash(&action);

        if (rule.action_metadata == NULL) {
            ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "[ysec_waf] Failed to resolve action");
            return NGX_CONF_ERROR;
        }

        action_metadata = (re_action_metadata*) rule.action_metadata;

        if (action_metadata->parse(cf, &value[n], &rule) != NGX_CONF_OK) {
            ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "[ysec_waf] Failed parsing '%V'", &action);
            return NGX_CONF_ERROR;
        }
    }

    if (rule.phase & REQUEST_HEADER_PHASE) {
        if (p->request_header_rules == NULL) {
            p->request_header_rules = ngx_array_create(cf->pool, 1, sizeof(ngx_http_yy_sec_waf_rule_t));

            if (p->request_header_rules == NULL)
                return NGX_CONF_ERROR;
        }

        rule_p = ngx_array_push(p->request_header_rules);

        if (rule_p == NULL)
            return NGX_CONF_ERROR;

        ngx_memcpy(rule_p, &rule, sizeof(ngx_http_yy_sec_waf_rule_t));
    }

    if (rule.phase & REQUEST_BODY_PHASE) {
        if (p->request_body_rules == NULL) {
            p->request_body_rules = ngx_array_create(cf->pool, 1, sizeof(ngx_http_yy_sec_waf_rule_t));

            if (p->request_body_rules == NULL)
                return NGX_CONF_ERROR;
        }

        rule_p = ngx_array_push(p->request_body_rules);

        if (rule_p == NULL)
            return NGX_CONF_ERROR;

        ngx_memcpy(rule_p, &rule, sizeof(ngx_http_yy_sec_waf_rule_t));
    }

    if (rule.phase & RESPONSE_HEADER_PHASE) {
        if (p->response_header_rules == NULL) {
            p->response_header_rules = ngx_array_create(cf->pool, 1, sizeof(ngx_http_yy_sec_waf_rule_t));

            if (p->response_header_rules == NULL)
                return NGX_CONF_ERROR;
        }

        rule_p = ngx_array_push(p->response_header_rules);

        if (rule_p == NULL)
            return NGX_CONF_ERROR;

        ngx_memcpy(rule_p, &rule, sizeof(ngx_http_yy_sec_waf_rule_t));
    }

    if (rule.phase & RESPONSE_BODY_PHASE) {
        if (p->response_body_rules == NULL) {
            p->response_body_rules = ngx_array_create(cf->pool, 1, sizeof(ngx_http_yy_sec_waf_rule_t));

            if (p->response_body_rules == NULL)
                return NGX_CONF_ERROR;
        }

        rule_p = ngx_array_push(p->response_body_rules);

        if (rule_p == NULL)
            return NGX_CONF_ERROR;

        ngx_memcpy(rule_p, &rule, sizeof(ngx_http_yy_sec_waf_rule_t));
    }

    // Temply hack here, create shm zone for conn processor and get server_ip.
    if (p->conn_processor) {
        shm_zone = ngx_http_yy_sec_waf_create_shm_zone(cf);

        if (shm_zone != NULL) {
            p->shm_zone = shm_zone;
        }
    }

    if (p->server_ip.len == 0) {

        p->server_ip.len = NGX_SOCKADDR_STRLEN;
        p->server_ip.data = ngx_pcalloc(cf->pool, NGX_SOCKADDR_STRLEN);
        
        if (p->server_ip.data == NULL) {
            return NGX_CONF_ERROR;
        }
    
        if (ngx_local_addr("eth0", &p->server_ip) != NGX_OK) {
            return NGX_CONF_ERROR;
        }
    }

    return NGX_CONF_OK;
}

/*
** @description: This function is called to read block list of yy sec waf.
** @para: ngx_conf_t *cf
** @para: ngx_command_t *cmd
** @para: void *conf
** @return: NGX_CONF_OK or NGX_CONF_ERROR if failed.
*/

char *
ngx_http_yy_sec_waf_re_block_list(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf)
{
    ngx_http_yy_sec_waf_loc_conf_t  *p = conf;

    ngx_str_t                        *value;
    ngx_regex_compile_t              *rgc;
    ngx_http_yy_sec_waf_block_list_t *block_list_p;

    value = cf->args->elts;

    /* variable */
    if (value[1].data[0] != '$') {
        return NGX_CONF_ERROR;
    }

    if (p->block_list == NULL) {
        p->block_list = ngx_array_create(cf->pool, 1, sizeof(ngx_http_yy_sec_waf_block_list_t));
        if (!p->block_list) {
            return NGX_CONF_ERROR;
        }
    }

	block_list_p = ngx_array_push(p->block_list);
    if (block_list_p == NULL) {
        return NGX_CONF_ERROR;
    }

    value[1].data++;
    value[1].len--;

	block_list_p->idx = ngx_http_get_variable_index(cf, &value[1]);
	if (block_list_p->idx == NGX_ERROR) {
		return NGX_CONF_ERROR;
	}

    /* regex */
    rgc = ngx_pcalloc(cf->pool, sizeof(ngx_regex_compile_t));
    if (!rgc)
        return NGX_CONF_ERROR;

    rgc->options = PCRE_CASELESS|PCRE_MULTILINE;
    rgc->pattern = value[2];
    rgc->pool = cf->pool;
    rgc->err.len = 0;
    rgc->err.data = NULL;

    block_list_p->regex = ngx_http_regex_compile(cf, rgc);
    if (block_list_p->regex == NULL)
        return NGX_CONF_ERROR;

    return NGX_CONF_OK;
}

/*
** @description: This function is called to read denied url of yy sec waf.
** @para: ngx_conf_t *cf
** @para: ngx_str_t *tmp
** @para: ngx_http_yy_sec_waf_rule_t *rule
** @return: NGX_CONF_OK or NGX_CONF_ERROR if failed.
*/

char *
ngx_http_yy_sec_waf_re_read_denied_url_conf(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf)
{
    ngx_http_yy_sec_waf_loc_conf_t *p = conf;
    ngx_str_t                      *value;
    ngx_int_t                       n;
    size_t                          size;
    ngx_file_t                      file;
    ngx_file_info_t                 fi;
    u_char                         *base;

    value = cf->args->elts;
    if (value[1].len == 0)
        return NGX_CONF_ERROR;

    if (p->denied_url.len != 0) {
        return NGX_CONF_OK;
    }

    file.name = value[1];
    file.log = cf->log;
    file.fd = ngx_open_file(file.name.data, NGX_FILE_RDONLY,
                            NGX_FILE_OPEN, NGX_FILE_DEFAULT_ACCESS);

    if (file.fd == NGX_INVALID_FILE) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, ngx_errno,
                      ngx_open_file_n " \"%s\" failed", file.name.data);
        return NGX_CONF_ERROR;
    }

    if (ngx_fd_info(file.fd, &fi) == NGX_FILE_ERROR) {
        ngx_conf_log_error(NGX_LOG_CRIT, cf, ngx_errno,
                           ngx_fd_info_n " \"%s\" failed", file.name.data);
        return NGX_CONF_ERROR;
    }

    size = (size_t) ngx_file_size(&fi);

    if (ngx_file_info(file.name.data, &fi) == NGX_FILE_ERROR) {
        ngx_conf_log_error(NGX_LOG_CRIT, cf, ngx_errno,
                           ngx_file_info_n " \"%s\" failed", file.name.data);
        return NGX_CONF_ERROR;
    }

    base = ngx_pcalloc(cf->pool, size);
    if (base == NULL) {
        return NGX_CONF_ERROR;
    }

    n = ngx_read_file(&file, base, size, 0);

    if (ngx_close_file(file.fd) == NGX_FILE_ERROR) {
        ngx_conf_log_error(NGX_LOG_ALERT, cf, ngx_errno,
                      ngx_close_file_n " \"%s\" failed", file.name.data);
    }

    if (n == NGX_ERROR) {
        return NGX_CONF_ERROR;
    }

    p->denied_url.data = base;
    p->denied_url.len = size;

    return NGX_CONF_OK;
}

/*
** @description: This function is called to create rule engine for yy sec waf.
** @para: ngx_conf_t *cf
** @return: NGX_OK or NGX_ERROR if failed.
*/

ngx_int_t
ngx_http_yy_sec_waf_re_create(ngx_conf_t *cf)
{
    rule_engine = ngx_pcalloc(cf->pool, sizeof(yy_sec_waf_re_t));
    if (rule_engine == NULL) {
        return NGX_ERROR;
    }

    if (ngx_http_yy_sec_waf_add_variables(cf) == NGX_ERROR)
        return NGX_ERROR;

    if (ngx_http_yy_sec_waf_init_operators_in_hash(cf,
            &rule_engine->operators_in_hash) == NGX_ERROR)
        return NGX_ERROR;

    if (ngx_http_yy_sec_waf_init_actions_in_hash(cf,
            &rule_engine->actions_in_hash) == NGX_ERROR)
        return NGX_ERROR;

    if (ngx_http_yy_sec_waf_init_tfns_in_hash(cf,
            &rule_engine->tfns_in_hash) == NGX_ERROR)
        return NGX_ERROR;

    return NGX_OK;
}

