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

    size_t         broker_size;
    size_t         nbrokers;
    ngx_str_t     *brokers;

} ngx_http_kafka_main_conf_t;

static void ngx_http_kafka_main_conf_broker_add(ngx_http_kafka_main_conf_t *cf,
        ngx_str_t *broker, ngx_pool_t *pool);

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

void ngx_http_kafka_main_conf_broker_add(ngx_http_kafka_main_conf_t *cf,
        ngx_str_t *broker, ngx_pool_t *pool)
{
    size_t       n;
    ngx_str_t   *new_brokers;

    for (n = 0; n != cf->nbrokers; ++n) {
        if (ngx_str_equal(broker, &cf->brokers[n])) {
            return;
        }
    }

    if (cf->nbrokers == cf->broker_size) {
        if (cf->broker_size == 0) {
            cf->broker_size = 2;
        }
        new_brokers = ngx_pcalloc(pool, sizeof(ngx_str_t) * (cf->broker_size * 2));
        if (new_brokers == NULL) {
            ngx_log_error(NGX_LOG_EMERG, pool->log, 0, "ngx no memory");
            return;
        }
        for (n = 0; n != cf->nbrokers; ++n) {
            new_brokers[n].len = cf->brokers[n].len;
            new_brokers[n].data = cf->brokers[n].data;
        }
        cf->brokers = new_brokers;
        cf->broker_size *= 2;
    }

    n = cf->nbrokers;

    /* 1 more byte for '\0' */
    cf->brokers[n].data = (u_char *)ngx_pcalloc(pool, sizeof(u_char) * (broker->len) + 1);
    if (cf->brokers[n].data == NULL) {
        ngx_log_error(NGX_LOG_EMERG, pool->log, 0, "ngx no memory");
    }
    cf->brokers[n].len = broker->len;

    ngx_memcpy(cf->brokers[n].data, broker->data, broker->len);
    cf->brokers[n].data[broker->len] = '\0';
    cf->nbrokers++;
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

    conf->broker_size = 0;
    conf->nbrokers = 0;
    conf->brokers = NULL;

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
    /* we can add more code here to config ngx_http_kafka_main_conf */
    return NGX_CONF_OK;
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
    ngx_http_kafka_main_conf_broker_add(main_conf, &local_conf->broker, cf->pool);

    return NGX_CONF_OK;
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
    static const char rc[] = "ngx_http_kafka_module ok\n";

    int                          nbufs;
    u_char                      *msg;
    size_t                       len;
    ngx_buf_t                   *buf;
    ngx_chain_t                  out;
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

    buf = ngx_pcalloc(r->pool, sizeof(ngx_buf_t));
    out.buf = buf;
    out.next = NULL;
    buf->pos = (u_char *)rc;
    buf->last = (u_char *)rc + sizeof(rc) - 1;
    buf->memory = 1;
    buf->last_buf = 1;

    ngx_str_set(&(r->headers_out.content_type), "text/html");
    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_length_n = sizeof(rc) - 1;
    ngx_http_send_header(r);
    ngx_http_output_filter(r, &out);
    ngx_http_finalize_request(r, NGX_OK);

    if (main_conf != NULL) {
        rd_kafka_poll(main_conf->rk, 0);
    }
}

ngx_int_t ngx_http_kafka_init_worker(ngx_cycle_t *cycle)
{
    size_t                       n;
    ngx_http_kafka_main_conf_t  *main_conf;

    main_conf = ngx_http_cycle_get_module_main_conf(cycle, ngx_http_kafka_module);

    main_conf->rkc = rd_kafka_conf_new();
    rd_kafka_conf_set_dr_cb(main_conf->rkc, kafka_callback_handler);

    main_conf->rk = rd_kafka_new(RD_KAFKA_PRODUCER, main_conf->rkc, NULL, 0);

    for (n = 0; n != main_conf->nbrokers; ++n) {
        ngx_str_helper(&main_conf->brokers[n], ngx_str_push);
        rd_kafka_brokers_add(main_conf->rk, (const char *)main_conf->brokers[n].data);
        ngx_str_helper(&main_conf->brokers[n], ngx_str_pop);
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


