FROM ubuntu:24.04

RUN apt-get update \
    && apt-get install -y --no-install-recommends build-essential cmake ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --target redisclone

EXPOSE 6379

CMD ["./build/redisclone", "--host", "0.0.0.0", "--port", "6379", "--data", "/app/data/dump.rdb"]

