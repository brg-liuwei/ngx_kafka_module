------------------
nginx kafka module
------------------

Q: What does this module do ?  
A: send post data to kafka

Q: How to install ?  
A: firstly, install librdkafka: https://github.com/edenhill/librdkafka

    git clone https://github.com/edenhill/librdkafka
    cd librdkafka
    ./configure
    make
    sudo make install

   then, compile this module into nginx

    git clone https://github.com/brg-liuwei/ngx_kafka_module
    # cd /path/to/nginx
    ./configure --add-module=/path/to/ngx_kafka_module
    make
    sudo make install
    # or, use `sudo make upgrade` instead of `sudo make install`

   thirdly, add the code to nginx conf file as follows

    http {

        # some other configs

        kafka;

        server {

            # some other configs

            location = /your/path/topic0/ {
                kafka_topic your_topic0;
                kafka_broker your_broker_addr0;   # eg: localhost:9092
            }

            location = /your/path/topic1/ {
                kafka_topic your_topic1;
                kafka_broker your_broker_addr1;   # eg: localhost:9092
            }
        }
    }

   then, reload your nginx

   test:

       curl localhost/your/path/topic0/ -d "message send to kafka topic0"
       curl localhost/your/path/topic1/ -d "message send to kafka topic1"

