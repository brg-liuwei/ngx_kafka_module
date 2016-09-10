use Test::Nginx::Socket 'no_plan';

repeat_each(1);

run_tests();

__DATA__

=== TEST 1: NOT ALLOW GET
--- http_config
    kafka;
    kafka_broker_list 127.0.0.1:9092;
--- config
    location /t {
        kafka_topic ngx-kafka-test-topic;
    }
--- request
GET /t
--- error_code: 405
--- no_error_log
[error]
