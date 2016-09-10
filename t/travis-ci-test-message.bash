#!/bin/bash

content=$(cat messages)
expected="hello nginx_kafka_module"
if [ "${content}" == "${expected}" ]; then
    echo "test kafka message ok"
    exit 0
fi

echo "$0 TEST FAIL:"
echo "    got: ${content}"
echo "    expected: ${expected}"
exit 1

