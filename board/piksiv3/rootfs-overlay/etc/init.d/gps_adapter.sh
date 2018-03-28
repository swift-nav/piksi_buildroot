#!/bin/sh
zmq_adapter --stdio -s '>tcp://127.0.0.1:44030' | socat pty,raw,link=/tmp/gps0 fd:0
