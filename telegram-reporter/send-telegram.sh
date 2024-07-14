#!/bin/bash

if [ -z "${TELEGRAM_TOKEN}" ]
then
    echo TELEGRAM_TOKEN is empty!
    exit -1
fi

if [ -z "${TELEGRAM_CHAT_ID}" ]
then
    echo TELEGRAM_CHAT_ID is empty!
    exit -2
fi

file=`cat`
#size=$(stat -c%s "$file.mp4")

ffmpeg -i "$file.mp4" -filter:v scale=720:-1 -c:a copy "$file.scale.mp4"

if [ $? -ne 0 ]
then
	rm ${file}.scale.mp4
	ffmpeg -i "$file.mp4" -filter:v scale=-1:480 -c:a copy "$file.scale.mp4"

	if [ $? -ne 0 ]
	then
		rm ${file}.scale.mp4
		set -x
	   	curl -F "video=@${file}.mp4" -X POST "https://api.telegram.org/bot${TELEGRAM_TOKEN}/sendVideo?chat_id=${TELEGRAM_CHAT_ID}"
		exit 0
	fi
fi

#if [ $size -gt 20971520 ]
#then
    set -x
    curl -F "video=@${file}.scale.mp4" -X POST "https://api.telegram.org/bot${TELEGRAM_TOKEN}/sendVideo?chat_id=${TELEGRAM_CHAT_ID}"
    rm ${file}.scale.mp4
#else
#    set -x
#    curl -o - -F "animation=@${file}.scale.mp4" -X POST "https://api.telegram.org/bot${TELEGRAM_TOKEN}/sendAnimation?chat_id=${TELEGRAM_CHAT_ID}"
#fi

rm ${file}.mp4