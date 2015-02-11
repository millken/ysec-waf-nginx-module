#include "ngx_yy_sec_waf.h"
#include <ifaddrs.h>

int
ngx_yy_sec_waf_unescape_uri(u_char **dst, u_char **src, size_t size, ngx_uint_t type);

/* 
** @description: Unescape routine.
** @para: ngx_str_t *str
** @return: uint (nullbytes+bad)
*/

int
ngx_yy_sec_waf_unescape(ngx_str_t *str) {
    u_char *dst, *src;
    u_int nullbytes = 0, i;
    
    dst = str->data;
    src = str->data;
        
    ngx_yy_sec_waf_unescape_uri(&dst, &src, str->len, 0);

    str->len = dst - str->data;

    /* tmp hack fix, avoid %00 & co (null byte) encoding :p */
    for (i = 0; i < str->len; i++) {
        if (str->data[i] == 0x0) {
    	    nullbytes++;
    	    //str->data[i] = '0';
        }
    }

    return nullbytes;
}

/*
** @description: Patched ngx_unescape_uri : 
** The original one does not care if the character following % is in valid range.
** For example, with the original one :
** '%uff' -> 'uff'
** @para: u_char **dst
** @para: u_char **src
** @para: size_t size
** @para: ngx_uint_t type
** @return: int
*/
int
ngx_yy_sec_waf_unescape_uri(u_char **dst, u_char **src, size_t size, ngx_uint_t type)
{
    u_char  *d, *s, ch, c, decoded;
    int bad = 0;
    
    enum {
        sw_usual = 0,
        sw_quoted,
        sw_quoted_second
    } state;

    d = *dst;
    s = *src;

    state = 0;
    decoded = 0;

    while (size--) {

        ch = *s++;

        switch (state) {
        case sw_usual:
            if (ch == '?'
                && (type & (NGX_UNESCAPE_URI|NGX_UNESCAPE_REDIRECT)))
            {
                *d++ = ch;
                goto done;
            }

            if (ch == '%') {
                state = sw_quoted;
                break;
            }

            // Convert + into space!
            if (ch == '+') {
                ch = ' ';
            }

            *d++ = ch;
            break;

        case sw_quoted:
	  
            if (ch >= '0' && ch <= '9') {
                decoded = (u_char) (ch - '0');
                state = sw_quoted_second;
                break;
            }
	    
            c = (u_char) (ch | 0x20);
            if (c >= 'a' && c <= 'f') {
                decoded = (u_char) (c - 'a' + 10);
                state = sw_quoted_second;
                break;
            }

            /* the invalid quoted character */
	    bad++;
            state = sw_usual;
	    *d++ = '%';
            *d++ = ch;
            break;

        case sw_quoted_second:

            state = sw_usual;

            if (ch >= '0' && ch <= '9') {
                ch = (u_char) ((decoded << 4) + ch - '0');

                if (type & NGX_UNESCAPE_REDIRECT) {
                    if (ch > '%' && ch < 0x7f) {
                        *d++ = ch;
                        break;
                    }

                    *d++ = '%'; *d++ = *(s - 2); *d++ = *(s - 1);

                    break;
                }

                *d++ = ch;

                break;
            }
	    
            c = (u_char) (ch | 0x20);
            if (c >= 'a' && c <= 'f') {
                ch = (u_char) ((decoded << 4) + c - 'a' + 10);

                if (type & NGX_UNESCAPE_URI) {
                    if (ch == '?') {
                        *d++ = ch;
                        goto done;
                    }

                    *d++ = ch;
                    break;
                }

                if (type & NGX_UNESCAPE_REDIRECT) {
                    if (ch == '?') {
                        *d++ = ch;
                        goto done;
                    }

                    if (ch > '%' && ch < 0x7f) {
                        *d++ = ch;
                        break;
                    }

                    *d++ = '%'; *d++ = *(s - 2); *d++ = *(s - 1);
                    break;
                }

                *d++ = ch;

                break;
            }
	    /* the invalid quoted character */
	    /* as it happened in the 2nd part of quoted character, 
	       we need to restore the decoded char as well. */
	    *d++ = '%';
	    *d++ = (0 >= decoded && decoded < 10) ? decoded + '0' : 
	      decoded - 10 + 'a';
	    *d++ = ch;
	    bad++;
            break;
        }
    }

done:

    *dst = d;
    *src = s;
    
    return (bad);
}

/* 
** @description: This function is called to convert ngx_int_t into u_char.
** @para: ngx_pool_t *p
** @para: ngx_int_t n
** @return: u_char*
*/

u_char*
ngx_yy_sec_waf_itoa(ngx_pool_t *p, ngx_int_t n)
{
    const int BUFFER_SIZE = sizeof(ngx_int_t) * 3 + 2;
    u_char *buf = ngx_palloc(p, BUFFER_SIZE);
	u_char *start = buf + BUFFER_SIZE - 1;
    int negative;

    if (n < 0) {
        negative = 1;
        n = -n;
    } else {
        negative = 0;
    }

    *start = 0;
    do {
        *--start = '0' + (n % 10);
        n /= 10;
    } while (n);

    if (negative) {
        *--start = '-';
    }

    return start;
}

/* 
** @description: This function is called to convert ngx_uint_t into u_char.
** @para: ngx_pool_t *p
** @para: ngx_uint_t n
** @return: u_char*
*/

u_char*
ngx_yy_sec_waf_uitoa(ngx_pool_t *p, ngx_uint_t n)
{
    const int BUFFER_SIZE = sizeof(ngx_uint_t) * 3 + 2;
    u_char *buf = ngx_palloc(p, BUFFER_SIZE);
	u_char *start = buf + BUFFER_SIZE - 1;

    *start = 0;
    do {
        *--start = '0' + (n % 10);
        n /= 10;
    } while (n);

    return start;
}

/* 
** @description: This function is called to get local addr.
** @para: ngx_connection_t *c
** @para: const char *eth
** @para: ngx_str_t *s
** @return: ngx_int_t
*/

ngx_int_t
ngx_local_addr(const char *eth, ngx_str_t *s)
{
    struct sockaddr_in  *addr4;
    struct sockaddr_in6 *addr6;
    struct ifaddrs      *ifap0, *ifap;

    if (eth == NULL || s == NULL) {
        return NGX_ERROR;
    }

    if (getifaddrs(&ifap0)) {
        return NGX_ERROR;
    }

    for (ifap = ifap0; ifap != NULL; ifap = ifap->ifa_next) {
        if(ngx_strcmp(eth, ifap->ifa_name)!=0)
            continue;

        if(ifap->ifa_addr == NULL)
			continue;

        if(AF_INET == ifap->ifa_addr->sa_family) {
            addr4 = (struct sockaddr_in *)ifap->ifa_addr;

            if((s->len = ngx_inet_ntop(ifap->ifa_addr->sa_family,
                (void *)&(addr4->sin_addr), s->data, s->len)) != 0) {

                freeifaddrs(ifap0);
                return NGX_OK;
            } else {
                break;
            }
        }
        else if(AF_INET6 == ifap->ifa_addr->sa_family) {
            addr6 = (struct sockaddr_in6*) ifap->ifa_addr;

            if(IN6_IS_ADDR_MULTICAST(&addr6->sin6_addr)) {
                continue;
            }

            if(IN6_IS_ADDR_LINKLOCAL(&addr6->sin6_addr)) {
                continue;
            }

            if(IN6_IS_ADDR_LOOPBACK(&addr6->sin6_addr)) {
                continue;
            }

            if(IN6_IS_ADDR_UNSPECIFIED(&addr6->sin6_addr)) {
                continue;
            }

            if(IN6_IS_ADDR_SITELOCAL(&addr6->sin6_addr)) {
                continue;
            }

            if((s->len = ngx_inet_ntop(ifap->ifa_addr->sa_family,
                (void *)&(addr6->sin6_addr), s->data, s->len)) != 0) {

                freeifaddrs(ifap0);
                return NGX_OK;
            }
            else {
                break;
            }

        } 
    }

    freeifaddrs(ifap0);

    return NGX_ERROR;
}


