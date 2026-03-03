FROM debian:bookworm

RUN apt update && apt install -y curl amqp-tools ffmpeg
COPY --chmod=0755 send-telegram.sh /usr/local/bin/

ENV ROUTE=person

ENTRYPOINT amqp-consume -e object -r $ROUTE -x -p 1 -u amqp://rabbitmq send-telegram.sh