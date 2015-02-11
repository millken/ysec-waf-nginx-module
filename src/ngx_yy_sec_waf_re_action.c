#include "ngx_yy_sec_waf_re.h"

/*
** @description: This function is called to parse gids of yy sec waf.
** @para: ngx_conf_t *cf
** @para: ngx_str_t *tmp
** @para: ngx_http_yy_sec_waf_rule_t *rule
** @return: NGX_CONF_OK or NGX_CONF_ERROR if failed.
*/

static void *
yy_sec_waf_parse_gids(ngx_conf_t *cf,
    ngx_str_t *tmp, ngx_http_yy_sec_waf_rule_t *rule)
{
    ngx_str_t *gids;

    if (!rule)
        return NGX_CONF_ERROR;

    gids = ngx_pcalloc(cf->pool, sizeof(ngx_str_t));
    if (!gids)
        return NGX_CONF_ERROR;

    gids->data = tmp->data + ngx_strlen(GIDS);
    gids->len = tmp->len - ngx_strlen(GIDS);

    rule->gids = gids;

    return NGX_CONF_OK;
}

/*
** @description: This function is called to parse rule id of yy sec waf.
** @para: ngx_conf_t *cf
** @para: ngx_str_t *tmp
** @para: ngx_http_yy_sec_waf_rule_t *rule
** @return: NGX_CONF_OK or NGX_CONF_ERROR if failed.
*/

static void *
yy_sec_waf_parse_rule_id(ngx_conf_t *cf,
    ngx_str_t *tmp, ngx_http_yy_sec_waf_rule_t *rule)
{
    ngx_str_t *rule_id;

    if (!rule)
        return NGX_CONF_ERROR;

    rule_id = ngx_pcalloc(cf->pool, sizeof(ngx_str_t));
    if (!rule_id)
        return NGX_CONF_ERROR;

    rule_id->data = tmp->data + ngx_strlen(ID);
    rule_id->len = tmp->len - ngx_strlen(ID);

    rule->rule_id = ngx_atoi(rule_id->data, rule_id->len);

    return NGX_CONF_OK;
}

/*
** @description: This function is called to parse msg of yy sec waf.
** @para: ngx_conf_t *cf
** @para: ngx_str_t *tmp
** @para: ngx_http_yy_sec_waf_rule_t *rule
** @return: NGX_CONF_OK or NGX_CONF_ERROR if failed.
*/

static void *
yy_sec_waf_parse_msg(ngx_conf_t *cf,
    ngx_str_t *tmp, ngx_http_yy_sec_waf_rule_t *rule)
{
    ngx_str_t *msg;

    if (!rule)
        return NGX_CONF_ERROR;

    msg = ngx_pcalloc(cf->pool, sizeof(ngx_str_t));
    if (!msg)
        return NGX_CONF_ERROR;

    msg->data = tmp->data + ngx_strlen(MSG);
    msg->len = tmp->len - ngx_strlen(MSG);

    rule->msg = msg;

    return NGX_CONF_OK;
}

/*
** @description: This function is called to parse level of yy sec waf.
** @para: ngx_conf_t *cf
** @para: ngx_str_t *tmp
** @para: ngx_http_yy_sec_waf_rule_t *rule
** @return: NGX_CONF_OK or NGX_CONF_ERROR if failed.
*/

static void *
yy_sec_waf_parse_level(ngx_conf_t *cf,
    ngx_str_t *tmp, ngx_http_yy_sec_waf_rule_t *rule)
{
    u_char *tmp_ptr;

    tmp_ptr = (u_char*)tmp->data + ngx_strlen(LEVEL);

    while (*tmp_ptr) {
        if (tmp_ptr[0] == '|')
            tmp_ptr++;
        /* match global zones */
        if (!ngx_strncasecmp(tmp_ptr, (u_char*)BLOCK, ngx_strlen(BLOCK))) {
            rule->action_level |= ACTION_BLOCK;
            tmp_ptr += ngx_strlen(BLOCK);
            continue;
        } else if (!ngx_strncasecmp(tmp_ptr, (u_char*)LOG, ngx_strlen(LOG))) {
            rule->action_level |= ACTION_LOG;
            tmp_ptr += ngx_strlen(LOG);
            continue;
        } else if (!ngx_strncasecmp(tmp_ptr, (u_char*)ALLOW, ngx_strlen(ALLOW))) {
            rule->action_level |= ACTION_ALLOW;
            tmp_ptr += ngx_strlen(ALLOW);
            continue;
        } else {
            rule->action_level &= ACTION_NONE;
            return (NGX_CONF_ERROR);
        }
    }

    return NGX_CONF_OK;
}

/*
** @description: This function is called to parse phase of yy sec waf.
** @para: ngx_conf_t *cf
** @para: ngx_str_t *tmp
** @para: ngx_http_yy_sec_waf_rule_t *rule
** @return: NGX_CONF_OK or NGX_CONF_ERROR if failed.
*/

static void *
yy_sec_waf_parse_phase(ngx_conf_t *cf,
    ngx_str_t *tmp, ngx_http_yy_sec_waf_rule_t *rule)
{

    u_char *tmp_ptr;

    if (!rule)
        return NGX_CONF_ERROR;

    tmp_ptr = (u_char*)tmp->data + ngx_strlen(PHASE);

    while (*tmp_ptr) {
        if (tmp_ptr[0] == ',')
            tmp_ptr++;
        /* match global zones */
        if (tmp_ptr[0] == '1') {
            rule->phase |= REQUEST_HEADER_PHASE;
            tmp_ptr += 1;
            continue;
        } else if (tmp_ptr[0] == '2') {
            rule->phase |= REQUEST_BODY_PHASE;
            tmp_ptr += 1;
        } else if (tmp_ptr[0] == '3') {
            rule->phase |= RESPONSE_HEADER_PHASE;
            tmp_ptr += 1;
        } else if (tmp_ptr[0] == '4') {
            rule->phase |= RESPONSE_BODY_PHASE;
            tmp_ptr += 1;
        } else {
            return NGX_CONF_ERROR;
        }
    }


    return NGX_CONF_OK;
}

/*
** @description: This function is called to parse tfn of yy sec waf.
** @para: ngx_conf_t *cf
** @para: ngx_str_t *tmp
** @para: ngx_http_yy_sec_waf_rule_t *rule
** @return: NGX_CONF_OK or NGX_CONF_ERROR if failed.
*/

static void *
yy_sec_waf_parse_tfn(ngx_conf_t *cf,
    ngx_str_t *tmp, ngx_http_yy_sec_waf_rule_t *rule)
{
    ngx_str_t *tfn;

    if (!rule)
        return NGX_CONF_ERROR;

    tfn = ngx_pcalloc(cf->pool, sizeof(ngx_str_t));
    if (!tfn)
        return NGX_CONF_ERROR;

    tfn->data = tmp->data + ngx_strlen(TFNS);
    tfn->len = tmp->len - ngx_strlen(TFNS);

    rule->tfn_metadata = yy_sec_waf_re_resolve_tfn_in_hash(tfn);

    return NGX_CONF_OK;
}

/*
** @description: This function is called to parse chain of yy sec waf.
** @para: ngx_conf_t *cf
** @para: ngx_str_t *tmp
** @para: ngx_http_yy_sec_waf_rule_t *rule
** @return: NGX_CONF_OK or NGX_CONF_ERROR if failed.
*/

static void *
yy_sec_waf_parse_chain(ngx_conf_t *cf,
    ngx_str_t *tmp, ngx_http_yy_sec_waf_rule_t *rule)
{
    ngx_str_t *chain;

    if (!rule)
        return NGX_CONF_ERROR;

    chain = ngx_pcalloc(cf->pool, sizeof(ngx_str_t));
    if (!chain)
        return NGX_CONF_ERROR;

    chain->data = tmp->data + ngx_strlen("chain:");
    chain->len = tmp->len - ngx_strlen("chain:");

    rule->is_chain = ngx_atoi(chain->data, chain->len);

    return NGX_CONF_OK;
}

/*
** @description: This function is called to parse status of yy sec waf.
** @para: ngx_conf_t *cf
** @para: ngx_str_t *tmp
** @para: ngx_http_yy_sec_waf_rule_t *rule
** @return: NGX_CONF_OK or NGX_CONF_ERROR if failed.
*/

static void *
yy_sec_waf_parse_status(ngx_conf_t *cf,
    ngx_str_t *tmp, ngx_http_yy_sec_waf_rule_t *rule)
{
    ngx_str_t *status;

    if (!rule)
        return NGX_CONF_ERROR;

    status = ngx_pcalloc(cf->pool, sizeof(ngx_str_t));
    if (!status)
        return NGX_CONF_ERROR;

    status->data = tmp->data + ngx_strlen("status:");
    status->len = tmp->len - ngx_strlen("status:");

    rule->status = ngx_atoi(status->data, status->len);

    return NGX_CONF_OK;
}

static re_action_metadata action_metadata[] = {
    { ngx_string("gids"), yy_sec_waf_parse_gids},
    { ngx_string("id"), yy_sec_waf_parse_rule_id},
    { ngx_string("msg"), yy_sec_waf_parse_msg},
    { ngx_string("lev"), yy_sec_waf_parse_level},
    { ngx_string("phase"), yy_sec_waf_parse_phase},
    { ngx_string("t"), yy_sec_waf_parse_tfn},
    { ngx_string("chain"), yy_sec_waf_parse_chain},
    { ngx_string("status"), yy_sec_waf_parse_status},
    { ngx_null_string, NULL}
};

/*
** @description: This function is called to init actions.
** @para: ngx_conf_t *cf
** @para: ngx_hash_t *actions_in_hash
** @return: NGX_OK or NGX_ERROR if failed.
*/

ngx_int_t
ngx_http_yy_sec_waf_init_actions_in_hash(ngx_conf_t *cf,
    ngx_hash_t *actions_in_hash)
{
    ngx_array_t         actions;
    ngx_hash_key_t     *hk;
    ngx_hash_init_t     hash;
    re_action_metadata     *metadata;

    if (ngx_array_init(&actions, cf->temp_pool, 32, sizeof(ngx_hash_key_t))
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    for (metadata = action_metadata; metadata->name.len; metadata++) {
        hk = ngx_array_push(&actions);
        if (hk == NULL) {
            return NGX_ERROR;
        }

        hk->key = metadata->name;
        hk->key_hash = ngx_hash_key_lc(metadata->name.data, metadata->name.len);
        hk->value = metadata;
    }

    hash.hash = actions_in_hash;
    hash.key = ngx_hash_key_lc;
    hash.max_size = 512;
    hash.bucket_size = ngx_align(64, ngx_cacheline_size);
    hash.name = "actions_in_hash";
    hash.pool = cf->pool;
    hash.temp_pool = NULL;

    if (ngx_hash_init(&hash, actions.elts, actions.nelts) != NGX_OK) {
        return NGX_ERROR;
    }

    return NGX_OK;
}

