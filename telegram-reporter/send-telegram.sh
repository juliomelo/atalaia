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

set -x
curl -o - -F "video=@${file}.mp4" -X POST "https://api.telegram.org/bot${TELEGRAM_TOKEN}/sendVideo?chat_id=${TELEGRAM_CHAT_ID}"