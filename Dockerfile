# ARIA DAW — Development Environment
FROM ubuntu:24.04

LABEL description="ARIA DAW Development Environment"

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    clang-17 \
    lld-17 \
    libasound2-dev \
    libpulse-dev \
    python3 \
    nodejs \
    npm \
    curl \
    git \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

# Install Rust
RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
ENV PATH="/root/.cargo/bin:${PATH}"

# Install pnpm
RUN npm install -g pnpm

WORKDIR /workspace
