FROM debian:bookworm-slim AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    make \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .
RUN make all

FROM debian:bookworm-slim
WORKDIR /app
COPY --from=builder /src/bin ./bin

EXPOSE 5555
CMD ["./bin/pcc_server", "5555"]
