# Lean Zephyr build image for latest upstream (SDK 1.0.1).
# Usage:
#   docker build -f docker/Dockerfile -t rzi-zephyr:latest .
#   For normal use, run ./scripts/container.sh from the RZI repository.
#   docker run --rm -it --network host --user "$(id -u):$(id -g)" \
#     -e HOME=/tmp -e ZEPHYR_BASE=/workdir/zephyr -v "$PWD:/workdir" -w /workdir \
#     rzi-zephyr:latest west build -b rak4631/nrf52840 /workdir/app

FROM ubuntu:24.04

ARG ZEPHYR_SDK_VERSION=1.0.1
# Match the WSL host UID and GID. Override them when building the image:
#   docker build --build-arg UID=$(id -u) --build-arg GID=$(id -g) ...
ARG UID=1001
ARG GID=1001
ARG USERNAME=daniel
ENV DEBIAN_FRONTEND=noninteractive \
    ZEPHYR_SDK_INSTALL_DIR=/opt/zephyr-sdk \
    ZEPHYR_TOOLCHAIN_VARIANT=zephyr \
    LANG=C.UTF-8

RUN apt-get update && apt-get install -y --no-install-recommends \
      git cmake ninja-build gperf ccache dfu-util device-tree-compiler \
      wget python3 python3-pip python3-venv python3-dev python3-tk \
      xz-utils file make gcc gcc-multilib g++-multilib libsdl2-dev libmagic1 \
      ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Put west on PATH so an interactive shell does not need to activate the virtual environment.
RUN python3 -m venv /opt/zephyr-venv \
    && /opt/zephyr-venv/bin/pip install --no-cache-dir -U pip west
ENV PATH="/opt/zephyr-venv/bin:${PATH}"

# Minimal SDK + host tools + x86 (qemu_x86) + arm (common boards)
RUN SDK_FILE="zephyr-sdk-${ZEPHYR_SDK_VERSION}_linux-x86_64_minimal.tar.xz" \
    && wget -q "https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v${ZEPHYR_SDK_VERSION}/${SDK_FILE}" \
    && tar xf "${SDK_FILE}" \
    && mv "zephyr-sdk-${ZEPHYR_SDK_VERSION}" "${ZEPHYR_SDK_INSTALL_DIR}" \
    && rm -f "${SDK_FILE}" \
    && cd "${ZEPHYR_SDK_INSTALL_DIR}" \
    && ./setup.sh -t x86_64-zephyr-elf -t arm-zephyr-eabi -h -c

# Install the Python packages required by Zephyr builds.
RUN wget -q -O /tmp/requirements-base.txt \
      https://raw.githubusercontent.com/zephyrproject-rtos/zephyr/main/scripts/requirements-base.txt \
    && /opt/zephyr-venv/bin/pip install --no-cache-dir -r /tmp/requirements-base.txt \
    && /opt/zephyr-venv/bin/pip install --no-cache-dir \
         cryptography intelhex click cbor2 \
    && rm -f /tmp/requirements-base.txt

# Match the container user to the host so it can write to the mounted workspace.
ARG UID
ARG GID
ARG USERNAME
RUN groupadd -g "${GID}" "${USERNAME}" \
    && useradd -m -u "${UID}" -g "${GID}" -s /bin/bash "${USERNAME}" \
    && chown -R "${UID}:${GID}" /opt/zephyr-venv
USER ${USERNAME}

WORKDIR /workdir
VOLUME ["/workdir"]

CMD ["bash"]
