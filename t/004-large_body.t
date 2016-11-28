use Test::Nginx::Socket 'no_plan';

repeat_each(1);

run_tests();

__DATA__


=== TEST 1: post 1023 data to kafka topic
--- http_config
    kafka;
    kafka_broker_list 127.0.0.1:9092;
--- config
    location /t {
        client_body_buffer_size 1024;
        kafka_topic ngx-kafka-test-topic;
    }
--- request eval
"POST /t
" . ('a' x 1023)
--- error_code: 204
--- no_error_log


=== TEST 2: post 1k chars to kafka topic
--- http_config
    kafka;
    kafka_broker_list 127.0.0.1:9092;
--- config
    location /t {
        client_body_buffer_size 1024;
        kafka_topic ngx-kafka-test-topic;
    }
--- request eval
"POST /t
" . ('a' x 1024)
--- error_code: 204
--- no_error_log


=== TEST 3: post 3000 chars to kafka topic when client_body_buffer_size is set 1024
--- http_config
    kafka;
    kafka_broker_list 127.0.0.1:9092;
--- config
    location /t {
        client_body_buffer_size 1024;
        kafka_topic ngx-kafka-test-topic;
    }
--- request eval
"POST /t
" . ('a' x 3000)
--- response_body
body_too_large
--- error_code: 500
--- error_log
ngx_http_kafka_handler cannot handle in-file-post-buf

