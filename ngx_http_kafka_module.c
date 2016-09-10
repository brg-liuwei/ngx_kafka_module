/*
 * nginx kafka module
 *
 * using librdkafka: https://github.com/edenhill/librdkafka
 */

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#include <librdkafka/rdkafka.h>

#define KAFKA_TOPIC_MAXLEN 256
#define KAFKA_BROKER_MAXLEN 512

static ngx_int_t ngx_http_kafka_init_worker(ngx_cycle_t *cycle);
static void ngx_http_kafka_exit_worker(ngx_cycle_t *cycle);

static void *ngx_http_kafka_create_main_conf(ngx_conf_t *cf);
static void *ngx_http_kafka_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_set_kafka(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *ngx_http_set_kafka_broker_list(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *ngx_http_set_kafka_topic(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *ngx_http_set_kafka_broker(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_int_t ngx_http_kafka_handler(ngx_http_request_t *r);
static void ngx_http_kafka_post_callback_handler(ngx_http_request_t *r);

typedef enum {
    ngx_str_push = 0,
    ngx_str_pop = 1
} ngx_str_op;

static void ngx_str_helper(ngx_str_t *str, ngx_str_op op);

typedef struct {
    rd_kafka_t       *rk;
    rd_kafka_conf_t  *rkc;
    ngx_array_t      *broker_list;
} ngx_http_kafka_main_conf_t;

static char *ngx_http_kafka_main_conf_broker_add(ngx_http_kafka_main_conf_t *cf,
        ngx_str_t *broker);

typedef struct {
    ngx_log_t  *log;
    ngx_str_t   topic;    /* kafka topic */
    ngx_str_t   broker;   /* broker addr (eg: localhost:9092) */

    rd_kafka_topic_t       *rkt;
    rd_kafka_topic_conf_t  *rktc;

} ngx_http_kafka_loc_conf_t;

static ngx_command_t ngx_http_kafka_commands[] = {
    {
        ngx_string("kafka"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_NOARGS,
        ngx_http_set_kafka,
        NGX_HTTP_MAIN_CONF_OFFSET,
        0,
        NULL },
    {
        ngx_string("kafka_broker_list"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_1MORE,
        ngx_http_set_kafka_broker_list,
        NGX_HTTP_MAIN_CONF_OFFSET,
        0,
        NULL },
    {
        ngx_string("kafka_topic"),
        NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_http_set_kafka_topic,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_kafka_loc_conf_t, topic),
        NULL },
    {
        ngx_string("kafka_broker"),
        NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_http_set_kafka_broker,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_kafka_loc_conf_t, broker),
        NULL },
    ngx_null_command
};

static ngx_http_module_t ngx_http_kafka_module_ctx = {
    NULL,      /* pre conf */
    NULL,      /* post conf */

    /* create main conf */
    ngx_http_kafka_create_main_conf,
    NULL,      /* init main conf */

    NULL,      /* create server conf */
    NULL,      /* init server conf */

    /* create local conf */
    ngx_http_kafka_create_loc_conf,
    NULL,      /* merge location conf */
};

ngx_module_t ngx_http_kafka_module = {
    NGX_MODULE_V1,
    &ngx_http_kafka_module_ctx, /* module context */
    ngx_http_kafka_commands,    /* module directives */
    NGX_HTTP_MODULE,            /* module type */

    NULL,          /* init master */
    NULL,          /* init module */
    ngx_http_kafka_init_worker,   /* init process */
    NULL,          /* init thread */
    NULL,          /* exit thread */
    ngx_http_kafka_exit_worker,   /* exit process */
    NULL,          /* exit master */

    NGX_MODULE_V1_PADDING
};

ngx_int_t ngx_str_equal(ngx_str_t *s1, ngx_str_t *s2)
{
    if (s1->len != s2->len) {
        return 0;
    }
    if (ngx_memcmp(s1->data, s2->data, s1->len) != 0) {
        return 0;
    }
    return 1;
}

char *ngx_http_kafka_main_conf_broker_add(ngx_http_kafka_main_conf_t *cf,
        ngx_str_t *broker)
{
    ngx_str_t   *new_broker;

    new_broker = ngx_array_push(cf->broker_list);
    if (new_broker == NULL) {
        return NGX_CONF_ERROR;
    }

    *new_broker = *broker;
    return NGX_OK;
}

void *ngx_http_kafka_create_main_conf(ngx_conf_t *cf)
{
    ngx_http_kafka_main_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_kafka_main_conf_t));
    if (conf == NULL) {
        return NGX_CONF_ERROR;
    }

    conf->rk = NULL;
    conf->rkc = NULL;
    conf->broker_list = ngx_array_create(cf->pool, 4, sizeof(ngx_str_t));
    if (conf->broker_list == NULL) {
        return NULL;
    }

    return conf;
}

void *ngx_http_kafka_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_kafka_loc_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_kafka_loc_conf_t));
    if (conf == NULL) {
        return NGX_CONF_ERROR;
    }
    conf->log = cf->log;
    ngx_str_null(&conf->topic);
    ngx_str_null(&conf->broker);

    return conf;
}

void kafka_callback_handler(rd_kafka_t *rk, void *msg, size_t len, int err, void *opaque, void *msg_opaque)
{
    if (err != 0) {
        ngx_log_error(NGX_LOG_ERR, (ngx_log_t *)msg_opaque, 0, rd_kafka_err2str(err));
    }
}

char *ngx_http_set_kafka(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    /* we can add more code here to config ngx_http_kafka_main_conf_t */
    return NGX_CONF_OK;
}

char *ngx_http_set_kafka_broker_list(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_uint_t  i;
    ngx_str_t  *value;

    ngx_http_kafka_main_conf_t *main_conf;

    main_conf = conf;
    value = cf->args->elts;

    for (i = 1; i < cf->args->nelts; ++i) {
        if (ngx_http_kafka_main_conf_broker_add(main_conf, &value[i]) == NGX_CONF_ERROR) {
            return NGX_CONF_ERROR;
        }
    }

    return NGX_OK;
}

char *ngx_http_set_kafka_topic(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t   *clcf;
    ngx_http_kafka_loc_conf_t  *local_conf;

    /* install ngx_http_kafka_handler */
    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    if (clcf == NULL) {
        return NGX_CONF_ERROR;
    }
    clcf->handler = ngx_http_kafka_handler;

    /* ngx_http_kafka_loc_conf_t::topic assignment */
    if (ngx_conf_set_str_slot(cf, cmd, conf) != NGX_CONF_OK) {
        return NGX_CONF_ERROR;
    }

    local_conf = conf;

    local_conf->rktc = rd_kafka_topic_conf_new();

    return NGX_CONF_OK;
}

char *ngx_http_set_kafka_broker(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_kafka_loc_conf_t  *local_conf;
    ngx_http_kafka_main_conf_t *main_conf;

    /* ngx_http_kafka_loc_conf_t::broker assignment */
    if (ngx_conf_set_str_slot(cf, cmd, conf) != NGX_CONF_OK) {
        return NGX_CONF_ERROR;
    }

    local_conf = conf;

    main_conf = ngx_http_conf_get_module_main_conf(cf, ngx_http_kafka_module);
    if (main_conf == NULL) {
        return NGX_CONF_ERROR;
    }
    return ngx_http_kafka_main_conf_broker_add(main_conf, &local_conf->broker);
}

static ngx_int_t ngx_http_kafka_handler(ngx_http_request_t *r)
{
    ngx_int_t  rv;

    if (!(r->method & NGX_HTTP_POST)) {
        return NGX_HTTP_NOT_ALLOWED;
    }

    rv = ngx_http_read_client_request_body(r, ngx_http_kafka_post_callback_handler);
    if (rv >= NGX_HTTP_SPECIAL_RESPONSE) {
        return rv;
    }

    return NGX_DONE;
}

static void ngx_http_kafka_post_callback_handler(ngx_http_request_t *r)
{
    int                          nbufs;
    u_char                      *msg;
    size_t                       len;
    ngx_chain_t                 *cl, *in;
    ngx_http_request_body_t     *body;
    ngx_http_kafka_main_conf_t  *main_conf;
    ngx_http_kafka_loc_conf_t   *local_conf;

    /* get body */
    body = r->request_body;
    if (body == NULL || body->bufs == NULL) {
        ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        return;
    }

    main_conf = NULL;

    /* calc len and bufs */
    len = 0;
    nbufs = 0;
    in = body->bufs;
    for (cl = in; cl != NULL; cl = cl->next) {
        nbufs++;
        len += (size_t)(cl->buf->last - cl->buf->pos);
    }

    /* get msg */
    if (nbufs == 0) {
        goto end;
    }

    if (nbufs == 1 && ngx_buf_in_memory(in->buf)) {
        msg = in->buf->pos;
    } else {
        if ((msg = ngx_pnalloc(r->pool, len)) == NULL) {
            ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
            return;
        }

        for (cl = in; cl != NULL; cl = cl->next) {
            if (ngx_buf_in_memory(cl->buf)) {
                msg = ngx_copy(msg, cl->buf->pos, cl->buf->last - cl->buf->pos);
            } else {
                /* TODO: handle buf in file */
                ngx_log_error(NGX_LOG_NOTICE, r->connection->log, 0,
                        "ngx_http_kafka_handler cannot handler in-file-post-buf");
                goto end;
            }
        }
        msg -= len;
    }

    /* send to kafka */
    main_conf = ngx_http_get_module_main_conf(r, ngx_http_kafka_module);
    local_conf = ngx_http_get_module_loc_conf(r, ngx_http_kafka_module);
    if (local_conf->rkt == NULL) {
        ngx_str_helper(&local_conf->topic, ngx_str_push);
        local_conf->rkt = rd_kafka_topic_new(main_conf->rk,
                (const char *)local_conf->topic.data, local_conf->rktc);
        ngx_str_helper(&local_conf->topic, ngx_str_pop);
    }

    /*
     * the last param should NOT be r->connection->log, for reason that
     * the callback handler ( func: kafka_callback_handler) would be called ASYNC-ly
     * when some errors being happened. At this time, 
     * ngx_http_finalize_request may have been invoked, in this case, the object r
     * had been destroyed but kafka_callback_handler use pointer r->connection->log.
     * DUANG! Worker process CRASH!
     *
     * Thanks for engineers of www.360buy.com report me this bug.
     * */
    rd_kafka_produce(local_conf->rkt, RD_KAFKA_PARTITION_UA, 
            RD_KAFKA_MSG_F_COPY, (void *)msg, len, NULL, 0, local_conf->log);

end:

    r->headers_out.status = NGX_HTTP_NO_CONTENT;
    ngx_http_send_header(r);
    ngx_http_finalize_request(r, NGX_OK);

    if (main_conf != NULL) {
        rd_kafka_poll(main_conf->rk, 0);
    }
}

ngx_int_t ngx_http_kafka_init_worker(ngx_cycle_t *cycle)
{
    ngx_uint_t                   n;
    ngx_str_t                   *broker_list;
    ngx_http_kafka_main_conf_t  *main_conf;

    main_conf = ngx_http_cycle_get_module_main_conf(cycle, ngx_http_kafka_module);
    main_conf->rkc = rd_kafka_conf_new();
    rd_kafka_conf_set_dr_cb(main_conf->rkc, kafka_callback_handler);
    main_conf->rk = rd_kafka_new(RD_KAFKA_PRODUCER, main_conf->rkc, NULL, 0);

    broker_list = main_conf->broker_list->elts;

    for (n = 0; n < main_conf->broker_list->nelts; ++n) {
        ngx_str_helper(&broker_list[n], ngx_str_push);
        rd_kafka_brokers_add(main_conf->rk, (const char *)broker_list[n].data);
        ngx_str_helper(&broker_list[n], ngx_str_pop);
    }

    return 0;
}

void ngx_http_kafka_exit_worker(ngx_cycle_t *cycle)
{
    ngx_http_kafka_main_conf_t  *main_conf;

    main_conf = ngx_http_cycle_get_module_main_conf(cycle, ngx_http_kafka_module);

    // TODO: rd_kafka_topic_destroy(each loc conf rkt );
    rd_kafka_destroy(main_conf->rk);
}

void ngx_str_helper(ngx_str_t *str, ngx_str_op op)
{
    static char backup;

    switch (op) {
        case ngx_str_push:
            backup = str->data[str->len];
            str->data[str->len] = 0;
            break;
        case ngx_str_pop:
            str->data[str->len] = backup;
            break;
        default:
            ngx_abort();
    }
}
