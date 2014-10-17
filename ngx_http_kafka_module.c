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

static ngx_int_t init_worker(ngx_cycle_t *cycle);
static void exit_worker(ngx_cycle_t *cycle);

static void *ngx_http_kafka_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_set_kafka(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *ngx_http_set_kafka_topic(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *ngx_http_set_kafka_broker(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_int_t ngx_http_kafka_handler(ngx_http_request_t *r);
static void ngx_http_kafka_post_callback_handler(ngx_http_request_t *r);

typedef struct {
    char topic[KAFKA_TOPIC_MAXLEN];
    char broker[KAFKA_BROKER_MAXLEN];

    rd_kafka_t            *rk;
    rd_kafka_conf_t       *rkc;
    rd_kafka_topic_t      *rkt;
    rd_kafka_topic_conf_t *rktc;

} ngx_http_kafka_real_conf_t;

static ngx_http_kafka_real_conf_t real_conf;

typedef struct {
    ngx_str_t topic;   /* kafka topic */
    ngx_str_t broker;  /* broker addr (eg: localhost:9092) */
} ngx_http_kafka_loc_conf_t;

static ngx_command_t ngx_http_kafka_commands[] = {
    {
        ngx_string("kafka"),
        NGX_HTTP_LOC_CONF|NGX_CONF_NOARGS,
        ngx_http_set_kafka,
        NGX_HTTP_LOC_CONF_OFFSET,
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

    NULL,      /* create main conf */
    NULL,      /* init main conf */

    NULL,      /* create server conf */
    NULL,      /* init server conf */

    ngx_http_kafka_create_loc_conf,  /* create local conf */
    NULL,                            /* merge location conf */
};

ngx_module_t ngx_http_kafka_module = {
    NGX_MODULE_V1,
    &ngx_http_kafka_module_ctx, /* module context */
    ngx_http_kafka_commands,    /* module directives */
    NGX_HTTP_MODULE,            /* module type */

    NULL,          /* init master */
    NULL,          /* init module */
    init_worker,   /* init process */
    NULL,          /* init thread */
    NULL,          /* exit thread */
    exit_worker,   /* exit process */
    NULL,          /* exit master */

    NGX_MODULE_V1_PADDING
};

static void *ngx_http_kafka_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_kafka_loc_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_kafka_loc_conf_t));
    if (conf == NULL) {
        return NGX_CONF_ERROR;
    }
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
      ngx_http_core_loc_conf_t   *clcf;

      /* install ngx_http_kafka_handler */
      clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
      if (clcf == NULL) {
          return NGX_CONF_ERROR;
      }
      clcf->handler = ngx_http_kafka_handler;

    real_conf.rk = NULL;
    real_conf.rkc = NULL;
    real_conf.rkt = NULL;
    real_conf.rktc = NULL;
    real_conf.rk = NULL;

    return NGX_CONF_OK;
}

char *ngx_http_set_kafka_topic(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_kafka_loc_conf_t  *local_conf;

    /* ngx_http_kafka_loc_conf_t::topic assignment */
    if (ngx_conf_set_str_slot(cf, cmd, conf) != NGX_CONF_OK) {
        return NGX_CONF_ERROR;
    }

    local_conf = conf;

    /* rdkafka topic */
    if (local_conf->topic.len >= KAFKA_TOPIC_MAXLEN) {
        return "kafka module topic too long";
    }

    /* convert ngx_string into char* */
    ngx_memcpy(real_conf.topic, local_conf->topic.data, local_conf->topic.len);
    real_conf.topic[local_conf->topic.len] = '\0';

    return NGX_CONF_OK;
}

char *ngx_http_set_kafka_broker(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_kafka_loc_conf_t  *local_conf;

    /* ngx_http_kafka_loc_conf_t::broker assignment */
    if (ngx_conf_set_str_slot(cf, cmd, conf) != NGX_CONF_OK) {
        return NGX_CONF_ERROR;
    }

    local_conf = conf;

    if (local_conf->broker.len >= KAFKA_BROKER_MAXLEN) {
        return NGX_CONF_ERROR;
    }

    ngx_memcpy(real_conf.broker, local_conf->broker.data, local_conf->broker.len);
    real_conf.broker[local_conf->broker.len] = '\0';

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

    int                         loop;
    int                         nevs, nbufs;
    u_char                     *msg;
    size_t                      len;
    ngx_buf_t                  *buf;
    ngx_chain_t                 out;
    ngx_chain_t                *cl, *in;
    ngx_http_request_body_t    *body;

    /* get body */
    body = r->request_body;
    if (body == NULL || body->bufs == NULL) {
        ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        return;
    }

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
                ngx_log_error(NGX_LOG_INFO, r->connection->log, 0,
                        "ngx_http_kafka_handler cannot handler in-file-post-buf");
                goto end;
            }
        }

        msg -= len;

    }

    /* send to kafka */
    rd_kafka_produce(real_conf.rkt, RD_KAFKA_PARTITION_UA, 
            RD_KAFKA_MSG_F_COPY, (void *)msg, len, NULL, 0, r->connection->log);

    rd_kafka_poll(real_conf.rk, 0);

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
}

static ngx_int_t init_worker(ngx_cycle_t *cycle)
{
    real_conf.rkc = rd_kafka_conf_new();
    rd_kafka_conf_set_dr_cb(real_conf.rkc, kafka_callback_handler);
    real_conf.rktc = rd_kafka_topic_conf_new();
    real_conf.rk = rd_kafka_new(RD_KAFKA_PRODUCER, real_conf.rkc, NULL, 0);
    real_conf.rkt = rd_kafka_topic_new(real_conf.rk, real_conf.topic, real_conf.rktc);
    rd_kafka_brokers_add(real_conf.rk, real_conf.broker);

    return 0;
}

static void exit_worker(ngx_cycle_t *cycle)
{
    rd_kafka_topic_destroy(real_conf.rkt);
    rd_kafka_destroy(real_conf.rk);
}




