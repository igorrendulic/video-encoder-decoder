#!/bin/sh

rm concat.txt

for file in `ls build/*.mp4`
do
    echo "file '$file'" >> concat.txt
done

ffmpeg -f concat -i concat.txt -c copy output.mp4
rm concat.txt
