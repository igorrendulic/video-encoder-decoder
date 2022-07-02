#!/bin/sh

sudo apt-get -y install libavcodec-dev \
    libavformat-dev libavdevice-dev \
    libswscale-dev libswresample-dev \
    libavfilter-dev libavutil-dev \
    libpostproc-dev

# install hiredis
git clone https://github.com/redis/hiredis.git
cd hiredis
make
make install
cp libhiredis.so /usr/local/lib
cd ..
rm -rf hiredis
sudo chmod 0755 /usr/local/lib/libhiredis.so
cd /usr/local/lib
sudo ldconfig