#!/bin/sh
 kill -9 `ps | grep GS0  | grep zmq_adapter | awk -F' ' '{print $1}'`
