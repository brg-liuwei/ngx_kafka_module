use Test::Nginx::Socket 'no_plan';

repeat_each(1);

run_tests();

__DATA__

=== TEST 1: post to kafka topic
--- http_config
    kafka;
    kafka_broker_list 127.0.0.1:9092;
--- config
    location /t {
        kafka_topic ngx-kafka-test-topic;
    }
--- request eval
"POST /t
hello nginx_kafka_module"
--- error_code: 204
--- no_error_log
[error]
