#!/bin/sh

# sudo apt update

sudo apt-get -y install libavcodec-dev \
    libavformat-dev libavdevice-dev \
    libswscale-dev libswresample-dev \
    libavfilter-dev libavutil-dev \
    libpostproc-dev

# install hiredis


sudo apt install redis
sudo systemctl status redis