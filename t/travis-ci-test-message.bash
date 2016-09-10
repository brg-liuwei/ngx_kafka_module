#!/bin/bash

content=$(cat messages)
echo $content
if [ $content == "hello nginx_kafka_module" ]; then
    echo "test kafka message ok"
    exit 0
fi

exit 1

