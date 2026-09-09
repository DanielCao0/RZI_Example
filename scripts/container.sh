#!/usr/bin/env bash
# Run this script with Bash.
# Wrapper for building the image, initializing west, opening a shell, and building firmware.
set -euo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
IMAGE="${IMAGE:-rzi-zephyr:latest}"
WS="$(dirname "${REPO}")"
if ! command -v docker >/dev/null 2>&1 && command -v docker.exe >/dev/null 2>&1; then
  docker() { command docker.exe "$@"; }
fi

usage() {
  cat <<EOF
Usage: $0 <command> [args...]

  build-image   Build docker/Dockerfile as ${IMAGE}
  init          Run west init -l app and west update (skip if initialized)
  shell         Open a shell in the container
  build         Run west build (initialize the workspace if needed)
                With no arguments, sysbuild the customer app for rzi_rak4631
  sample        Build the upstream usp_zephyr periodical_uplink sample
                for nRF52840 DK plus an SX126x shield
  run           Run west build -t run
  clangd        Rewrite compile_commands paths for host clangd
  patch         Apply the patches owned by RZI (see doc/west-patch.md)
  patch-list    List the patches owned by RZI
  Flash: ./scripts/flash-rak4631.sh

Examples:
  $0 build-image
  $0 build
  $0 sample
  $0 build -p always --sysbuild -b rzi_rak4631/nrf52840 /workdir/app
  $0 shell
EOF
}

# west -p always invokes cmake/pristine.cmake through the cached ZEPHYR_BASE.
# A cache produced on the host may contain a /home/... path that is unavailable in the container.
# Remove the host build directory so the container always configures from an empty directory.
wipe_host_build_dir() {
  local dir="$1"
  if [[ -d "$dir" ]]; then
    rm -rf "$dir"
  fi
}

# Docker Desktop on WSL needs Windows paths and a bind at /rzi: the
# workspace symlink rzi -> ../rzi becomes /workdir/rzi -> /rzi inside
# the container, and a bind on the symlink is not overlaid.
host_vol_path() {
  local p="$1"
  if command -v wslpath >/dev/null 2>&1 && grep -qi microsoft /proc/version 2>/dev/null; then
    wslpath -w "$p"
  else
    printf '%s\n' "$p"
  fi
}

publish_merged_hex() {
  local dir="$1"
  local found
  found="$(compgen -G "${dir}/merged_*.hex" || true)"
  if [[ -n "${found}" ]]; then
    cp ${found} "${dir}/merged.hex"
  fi
}

run_docker() {
  local workdir="${DOCKER_WORKDIR:-/workdir}"
  local it=()
  local module_mounts=()
  local ws_vol rzi_real
  if [[ -t 0 ]]; then it=(-it); fi
  ws_vol="$(host_vol_path "${WS}")"
  if [[ -L "${WS}/rzi" ]]; then
    rzi_real="$(readlink -f "${WS}/rzi")"
    module_mounts=(
      -v "$(host_vol_path "${rzi_real}"):/workdir/rzi"
      -v "$(host_vol_path "${rzi_real}"):/rzi"
    )
  fi
  docker run --rm "${it[@]}" --network host \
    --user "$(id -u):$(id -g)" \
    -e HOME=/tmp \
    -e ZEPHYR_BASE=/workdir/zephyr \
    -e GIT_CONFIG_COUNT=1 \
    -e GIT_CONFIG_KEY_0=http.version \
    -e GIT_CONFIG_VALUE_0=HTTP/1.1 \
    -v "${ws_vol}:/workdir" \
    "${module_mounts[@]}" \
    -w "${workdir}" \
    "${IMAGE}" \
    "$@"
}

# Use app/west.yml as the customer application manifest.
ensure_workspace() {
  if [[ -d "${WS}/.west" ]]; then
    return 0
  fi
  if [[ ! -f "${REPO}/west.yml" ]]; then
    echo "missing ${REPO}/west.yml" >&2
    exit 1
  fi
  echo "First-time setup: west init -l app + west update (this takes a while)..."
  DOCKER_WORKDIR=/workdir run_docker bash -lc '
    set -e
    west init -l app
    n=0
    until west update; do
      n=$((n+1))
      if [ "$n" -ge 5 ]; then
        echo "west update still failing after 5 tries" >&2
        exit 1
      fi
      echo "west update retry $n ..."
      sleep 5
    done
    west zephyr-export || true
  '
  west_patch_apply
}

# Apply the west patch manifest owned by the RZI module.
west_patch_apply() {
  if [[ ! -f "${WS}/rzi/zephyr/patches.yml" || ! -d "${WS}/.west" ]]; then
    return 0
  fi
  run_docker west patch -sm rzi clean
  run_docker west patch -sm rzi apply --roll-back
}

west_patch_list() {
  run_docker west patch -sm rzi list
}

# Translate container paths and compiler names for clangd running on the host.
sync_compile_commands() {
  local src="${1:-${WS}/build/app/compile_commands.json}"
  local dst="${REPO}/compile_commands.json"
  if [[ ! -f "${src}" ]]; then
    return 0
  fi
  python3 - "${WS}" "${src}" "${dst}" <<'PY'
import json, re, sys
root, src, dst = sys.argv[1], sys.argv[2], sys.argv[3]
host_cc = "arm-none-eabi-gcc"
docker_cc = "/opt/zephyr-sdk/gnu/arm-zephyr-eabi/bin/arm-zephyr-eabi-gcc"
with open(src, encoding="utf-8") as f:
    data = json.load(f)
for e in data:
    for k in ("directory", "file", "command", "output"):
        if k in e and isinstance(e[k], str):
            e[k] = e[k].replace("/workdir", root)
    if "command" in e:
        e["command"] = e["command"].replace(docker_cc, host_cc)
        e["command"] = re.sub(r" --sysroot=\S+", "", e["command"])
        e["command"] = re.sub(r" -specs=\S+", "", e["command"])
with open(dst, "w", encoding="utf-8") as f:
    json.dump(data, f, indent=1)
    f.write("\n")
print(f"clangd db: {dst} ({len(data)} files)")
PY
}

cmd="${1:-}"
case "${cmd}" in
  build-image)
    docker build -f "${REPO}/docker/Dockerfile" \
      --build-arg UID="$(id -u)" \
      --build-arg GID="$(id -g)" \
      --build-arg USERNAME="$(id -u -n)" \
      -t "${IMAGE}" "${REPO}"
    ;;
  init)
    if [[ -d "${WS}/.west" ]]; then
      echo "Already initialized: ${WS}"
      west_patch_apply
      exit 0
    fi
    ensure_workspace
    ;;
  shell)
    shift || true
    ensure_workspace
    west_patch_apply
    run_docker bash "$@"
    ;;
  build)
    shift || true
    ensure_workspace
    west_patch_apply
    if [[ $# -eq 0 ]]; then
      wipe_host_build_dir "${WS}/build/app"
      run_docker bash -lc 'python3 -c "import cryptography" 2>/dev/null || pip install --no-cache-dir cryptography cbor2
        west build -p always --sysbuild -b rzi_rak4631/nrf52840 -d /workdir/build/app /workdir/app'
      publish_merged_hex "${WS}/build/app"
    else
      run_docker west build "$@"
    fi
    sync_compile_commands "${WS}/build/app/compile_commands.json"
    ;;
  sample)
    ensure_workspace
    west_patch_apply
    wipe_host_build_dir "${WS}/build/usp-periodical-uplink"
    run_docker west build -p always \
      -b nrf52840dk/nrf52840 \
      --shield semtech_sx1261mb2bas \
      -d /workdir/build/usp-periodical-uplink \
      usp_zephyr/samples/usp/lbm/periodical_uplink
    sync_compile_commands "${WS}/build/usp-periodical-uplink/compile_commands.json"
    ;;
  clangd)
    sync_compile_commands "${WS}/build/app/compile_commands.json"
    ;;
  patch)
    west_patch_apply
    ;;
  patch-list)
    west_patch_list
    ;;
  run)
    shift || true
    ensure_workspace
    run_docker west build -t run "$@"
    ;;
  -h|--help|"")
    usage
    ;;
  *)
    usage
    exit 1
    ;;
esac
