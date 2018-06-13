#!/usr/bin/env bash

if [ -z ${AWS_ACCESS_KEY_ID+x} ]; then
    echo "AWS_ACCESS_KEY_ID not set"
    echo "Check that the PR wasn't scheduled from a fork since forks do not have access to AWS credentials"
    exit 1
fi
