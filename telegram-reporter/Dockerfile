FROM debian:bookworm

RUN apt update && apt install -y curl amqp-tools ffmpeg
COPY send-telegram.sh /usr/local/bin
ENTRYPOINT amqp-consume -e object -r person -x -p 1 -u amqp://rabbitmq send-telegram.sh