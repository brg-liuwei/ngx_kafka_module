------------------
nginx kafka module
------------------

Q: What does this module do ?  
A: send post data to kafka

Q: How to install  
A: firstly, install librdkafka: https://github.com/edenhill/librdkafka

    git clone https://github.com/edenhill/librdkafka
    cd librdkafka
    ./configure
    make
    sudo make install

   then, compile this module into nginx

    git clone https://github.com/brg-liuwei/ngx_http_kafka_module
    # cd /path/to/nginx
    ./configure --add-module=/path/to/ngx_http_kafka_module
    make
    sudo make install
    # or, use `sudo make upgrade` instead of `sudo make install`

   thirdly, add the code to nginx conf file as follows

    location = /your/path/ {
        kafka;
        kafka_topic your_topic;
        kafka_broker your_broker_addr;   # eg: localhost:9092
    }

   then, reload your nginx

   test:

       curl localhost/your/path/ -d "message send to kafka"

