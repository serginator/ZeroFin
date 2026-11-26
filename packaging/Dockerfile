# Build image for the CP0: Debian 13 arm64, the same libc and libmpv as the device.
FROM debian:trixie
RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential cmake ninja-build git ca-certificates xz-utils pkg-config file \
      libmpv-dev libasound2-dev libfreetype-dev \
    && rm -rf /var/lib/apt/lists/*
