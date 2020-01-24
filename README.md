# Atalaia

Video Intelligence Pipeline

This is a prototype of a video intelligence pipeline software for surveillance.

## Features

* Moving person detection
* Telegram notification by video

## How to use it

1. Create ~/atalaia.env containing:

   ```
   ATALAIA_VIDEO_URL=rtsp://user:password@ip2/path1 rtsp://user:password@ip2/path2 ...
   TELEGRAM_TOKEN=token from telegram for a bot
   TELEGRAM_CHAT_ID=telegram's chat id to send video alerts
   ```

2. Start it using `docker-compose up`
