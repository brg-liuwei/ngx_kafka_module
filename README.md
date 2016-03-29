------------------
# nginx kafka module
------------------

### Synopsis

This nginx module is used to receive http post data and delivery messages to [kafka](http://kafka.apache.org/)

### Installation

* Dependency Installation

    Install [librdkafka](https://github.com/edenhill/librdkafka)

        git clone https://github.com/edenhill/librdkafka
        cd librdkafka
        ./configure
        make
        sudo make install

* Compilation

    Compile this module into nginx

        git clone https://github.com/brg-liuwei/ngx_kafka_module

        # cd /path/to/nginx
        ./configure --add-module=/path/to/ngx_kafka_module

        make

        sudo make install
        # or, use `sudo make upgrade` instead of `sudo make install`

### Nginx Configuration

    add the code to nginx conf file as follows

    http {

        # some other configs

        kafka;

        kafka_broker_list 127.0.0.1:9092 127.0.0.1:9093; # host:port ...

        server {

            # some other configs

            location = /your/path/topic0/ {
                kafka_topic your_topic0;
            }

            location = /your/path/topic1/ {
                kafka_topic your_topic1;
            }
        }
    }


### Example:

    curl localhost/your/path/topic0/ -d "message send to kafka topic0"
    curl localhost/your/path/topic1/ -d "message send to kafka topic1"

