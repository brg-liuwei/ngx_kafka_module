Nginx Kafka Module
====

Nginx kafka module is used to receive http post data and deliver messages to [kafka](http://kafka.apache.org/)

If there are any problems when using this module, feel free to send a mail to me :)

Table of Contents
====

* [Nginx Kafka Module](#nginx-kafka-module)
* [Installation](#installation)
    * [Bootstrapping](#bootstrapping)
    * [Compilation](#compilation)
* [NGINX Configuration](#nginx-configuration)
* [Starting and Stopping NGINX](#starting-and-stopping-nginx)
* [Example of Usage](#example-of-usage)
* [Report Bugs](#report-bugs)
* [Copyright & License](#copyright--license)

Installation
====

This module has a Rake-based toolchain to simplify quickly getting the
module running in an NGINX instance. If you do not have
[Bundler](http://bundler.io/) installed, get it now with `gem install
bundler`.  Next, inside your clone of the repository do `bundle
install`.

At this point, you can see the commands available to you:
```
$ rake -T
rake bootstrap        # Bootstraps the local development environment
rake bootstrap:clean  # Removes vendor code
rake nginx:compile    # Recompiles NGINX
rake nginx:start      # Starts NGINX
rake nginx:stop       # Stops NGINX
```

Bootstrapping
----

Doing `rake bootstrap` will download, build, and install NGINX and librdkafka
for you.

Doing `rake bootstrap:clean` removes source and installs of NGINX and
librdkafka.

[Back to TOC](#table-of-contents)

Compilation
----

Compile this module into NGINX by doing `rake nginx:compile`.

[Back to TOC](#table-of-contents)

NGINX Configuration
====

The [nginx.conf](nginx.conf) file demonstrates how to configure the module, and
that file is used in the NGINX that was built in the previous step.

[Back to TOC](#table-of-contents)

Starting and Stopping NGINX
====

Once it has been built, you can start and stop NGINX with the following
commands.
```
$ rake nginx:start
$ rake nginx:stop
```

Example of Usage
====

All that's left now to see the module in action is to get Kafka running and
make POST requests to the server.

Follow the instructions in the [Kafka Quickstart](http://kafka.apache.org/documentation.html#quickstart)
to get Zookeeper and Kafka running. Then create a topic named "test". Finally,
you will want to start a consumer so you can see the messages being written to
the topic. This is also shown in the Kafka Quickstart and looks something like:
```
$ kafka-console-consumer --zookeeper localhost:2181 --topic test --from-beginning
```

At this point, you should have a running NGINX that was built with the Kafka
module and a running Kafka instance that you are watching. Now it's time to
make requests!

```
$ curl localhost:8080/publish -d "it's working"
```

If all goes as planned, you will see "it's working" printed in the terminal
that is running the `kafka-console-consumer` command.

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
