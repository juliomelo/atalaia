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

# Se o arquivo não tiver extensão, adiciona .mp4
if [[ "$file" == *.* ]]; then
    file_with_ext="$file"
    file_base="${file%.*}"
else
    file_with_ext="${file}.mp4"
    file_base="$file"
fi

#size=$(stat -c%s "$file_with_ext")

rm -f ${file_base}.movements

ffmpeg -i "$file_with_ext" -filter:v scale=720:-1 -c:a copy "$file_base.scale.mp4"

if [ $? -ne 0 ]
then
	rm -f ${file_base}.scale.mp4
	ffmpeg -i "$file_with_ext" -filter:v scale=-1:480 -c:a copy "$file_base.scale.mp4"

	if [ $? -ne 0 ]
	then
		rm -f ${file_base}.scale.mp4
		set -x
	   	curl -F "video=@${file_with_ext}" -X POST "https://api.telegram.org/bot${TELEGRAM_TOKEN}/sendVideo?chat_id=${TELEGRAM_CHAT_ID}"
		exit 0
	fi
fi

#if [ $size -gt 20971520 ]
#then
    set -x
    curl -F "video=@${file_base}.scale.mp4" -X POST "https://api.telegram.org/bot${TELEGRAM_TOKEN}/sendVideo?chat_id=${TELEGRAM_CHAT_ID}"
    rm -f ${file_base}.scale.mp4
#else
#    set -x
#    curl -o - -F "animation=@${file_base}.scale.mp4" -X POST "https://api.telegram.org/bot${TELEGRAM_TOKEN}/sendAnimation?chat_id=${TELEGRAM_CHAT_ID}"
#fi

rm -f ${file_with_ext}