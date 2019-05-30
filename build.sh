#!/bin/bash
g++ HttpDownloader.cpp main.cpp -o HttpServerDownloader -std=c++11 -g
g++ http_server_tcp_client.cpp -o client -std=c++11 -g
