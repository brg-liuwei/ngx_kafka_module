#!/bin/bash

content=$(cat messages)
if [[ $content != "hello nginx_kafka_module" ]]; then
    exit 1
fi

echo "test kafka message ok"
