#!/bin/bash
mkdir -p bin
g++ -O3 -std=c++17 \
    src/vfs_client.cpp \
    example/main.cpp \
    -I./include \
    -I./vendor \
    -lpthread \
    -o bin/vfs_example_agent
