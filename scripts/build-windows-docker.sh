#!/usr/bin/env bash
# scripts/build-windows-docker.sh
# Builds OpenArch for Windows x86_64 in an isolated Docker/Podman container.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

# Detect container engine: docker or podman
CONTAINER_ENGINE=""
if command -v docker >/dev/null 2>&1; then
    CONTAINER_ENGINE="docker"
elif command -v podman >/dev/null 2>&1; then
    CONTAINER_ENGINE="podman"
else
    echo "Error: Neither 'docker' nor 'podman' was found in PATH." >&2
    echo "Please install Docker or Podman to use containerized cross-compilation." >&2
    exit 1
fi

IMAGE_NAME="openarch-win64-builder:latest"
DOCKERFILE="${ROOT_DIR}/docker/Dockerfile.windows"

if [ ! -f "${DOCKERFILE}" ]; then
    echo "Error: Dockerfile not found at ${DOCKERFILE}" >&2
    exit 1
fi

echo "================================================================="
echo " OpenArch Windows x86_64 Cross-Compiler (Isolated Container)"
echo " Engine:    ${CONTAINER_ENGINE}"
echo " Workspace: ${ROOT_DIR}"
echo " Image:     ${IMAGE_NAME}"
echo "================================================================="

# 1. Build the container image
echo ""
echo "[Step 1/2] Building container image '${IMAGE_NAME}'..."
"${CONTAINER_ENGINE}" build \
    --progress=plain \
    -t "${IMAGE_NAME}" \
    -f "${DOCKERFILE}" \
    "${ROOT_DIR}"

# 2. Run the build and packager inside the container
echo ""
echo "[Step 2/2] Running build and package inside isolated container..."
# If using podman, add :z flag for SELinux relabeling if needed
VOLUME_FLAGS="-v ${ROOT_DIR}:/workspace"
if [ "${CONTAINER_ENGINE}" = "podman" ]; then
    VOLUME_FLAGS="-v ${ROOT_DIR}:/workspace:z"
fi

"${CONTAINER_ENGINE}" run --rm \
    ${VOLUME_FLAGS} \
    -e HOST_UID="$(id -u)" \
    -e HOST_GID="$(id -g)" \
    "${IMAGE_NAME}"

echo ""
echo "================================================================="
echo " Windows Cross-Compilation Finished Successfully!"
echo " Output artifacts:"
if [ -f "${ROOT_DIR}/build-win64/OpenArch-Setup-x86_64.exe" ]; then
    echo "   NSIS Installer: ${ROOT_DIR}/build-win64/OpenArch-Setup-x86_64.exe"
    ls -lh "${ROOT_DIR}/build-win64/OpenArch-Setup-x86_64.exe"
fi
if [ -f "${ROOT_DIR}/build-win64/OpenArch-windows-x86_64.zip" ]; then
    echo "   Portable Zip:   ${ROOT_DIR}/build-win64/OpenArch-windows-x86_64.zip"
    ls -lh "${ROOT_DIR}/build-win64/OpenArch-windows-x86_64.zip"
fi
if [ -d "${ROOT_DIR}/build-win64/dist" ]; then
    echo "   Binaries Dir:   ${ROOT_DIR}/build-win64/dist/"
    ls -lh "${ROOT_DIR}/build-win64/dist"/*.exe 2>/dev/null || true
fi
echo "================================================================="

