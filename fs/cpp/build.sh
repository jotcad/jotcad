#!/bin/bash
mkdir -p bin
g++ -O3 -std=c++17 \
    vfs_client.cpp \
    example/main.cpp \
    -I./ \
    -I./vendor \
    -lpthread \
    -o bin/vfs_example_agent
