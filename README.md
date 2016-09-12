Nginx Kafka Module
====

[![Build Status](https://travis-ci.org/brg-liuwei/ngx_kafka_module.svg?branch=master)](https://travis-ci.org/brg-liuwei/ngx_kafka_module)

Nginx kafka module is used to receive http post data and deliver messages to [kafka](http://kafka.apache.org/)

If there are any problems when using this module, feel free to send a mail to me :)

Table of Contents
====

* [Nginx Kafka Module](#nginx-kafka-module)
* [Installation](#installation)
    * [Dependency Installation](#dependency-installation)
    * [Compilation](#compilation)
* [Nginx Configuration](#nginx-configuration)
* [Example of Usage](#example-of-usage)
* [Report Bugs](#report-bugs)
* [Copyright & License](#copyright--license)

Installation
====

Dependency Installation
----

Install [librdkafka](https://github.com/edenhill/librdkafka)

    git clone https://github.com/edenhill/librdkafka
    cd librdkafka
    ./configure
    make
    sudo make install

Compilation
----

Compile this module into nginx

    git clone https://github.com/brg-liuwei/ngx_kafka_module

    # cd /path/to/nginx
    ./configure --add-module=/path/to/ngx_kafka_module

    make

    sudo make install
    # or, use `sudo make upgrade` instead of `sudo make install`

In newer version of nginx(>=1.9.11), you can make this module as an dynamic module.

    git clone https://github.com/brg-liuwei/ngx_kafka_module

    # cd /path/to/nginx
    ./configure --add-dynamic-module=/path/to/ngx_kafka_module

    make modules
    # This will generate a objs/ngx_http_kafka_module.so in your /path/to/ngx_kafka_module and you can copy the so file to a proper location.

[Back to TOC](#table-of-contents)

Nginx Configuration
====

Add the code to nginx conf file as follows

    http {

        # some other configs

        kafka;

        kafka_broker_list 127.0.0.1:9092 127.0.0.1:9093; # host:port ...

        server {

            # some other configs

            location = /your/path/topic0 {
                kafka_topic your_topic0;
            }

            location = /your/path/topic1 {
                kafka_topic your_topic1;
            }
        }
    }

If you compile module as a dynamic module, you must add
    
    load_module /path/to/ngx_http_kafka_module.so;

at the beginning of the nginx config file besides adding the code. After that you can use the module by just executing `nginx -c /path/to/nginx.conf -s reload`.


[Back to TOC](#table-of-contents)

Example of Usage
====

    curl localhost/your/path/topic0 -d "message send to kafka topic0"
    curl localhost/your/path/topic1 -d "message send to kafka topic1"

[Back to TOC](#table-of-contents)

Report Bugs
====

You are very welcome to report issues on GitHub:

https://github.com/brg-liuwei/ngx_kafka_module/issues

[Back to TOC](#table-of-contents)

Copyright & License
====

This module is licensed under the BSD license.

Copyright (C) 2014-2016, by Liu Wei(brg-liuwei) stupidlw@126.com.

All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

[Back to TOC](#table-of-contents)
