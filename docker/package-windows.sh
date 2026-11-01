#!/usr/bin/env bash
# docker/package-windows.sh
# In-container cross-compilation and automated deployment for OpenArch on Windows x86_64

set -euo pipefail

WORKSPACE="/workspace"
BUILD_DIR="${WORKSPACE}/build-win64"
DIST_DIR="${BUILD_DIR}/dist"
SYSROOT_BIN="/usr/x86_64-w64-mingw32/sys-root/mingw/bin"

echo "=========================================================="
echo " Starting OpenArch Windows x86_64 Cross-Compilation"
echo "=========================================================="

# 1. Configure with Fedora's MinGW CMake toolchain
echo "[1/6] Configuring CMake for MinGW-w64..."
TOOLCHAIN_FILE="/usr/share/mingw/toolchain-mingw64.cmake"
if [ ! -f "${TOOLCHAIN_FILE}" ]; then
    TOOLCHAIN_FILE=$(find /usr -name "toolchain-mingw64.cmake" 2>/dev/null | head -n 1)
fi

cmake \
    -B "${BUILD_DIR}" \
    -S "${WORKSPACE}" \
    -G "Unix Makefiles" \
    -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}" \
    -DCMAKE_BUILD_TYPE=Release

# 2. Build application targets
echo "[2/6] Building targets in parallel..."
cmake --build "${BUILD_DIR}" --target openarch-gui openarch openarch-theme-editor -j"$(nproc)"

# 3. Prepare distribution folder
echo "[3/6] Setting up distribution structure in ${DIST_DIR}..."
rm -rf "${DIST_DIR}"
mkdir -p "${DIST_DIR}"
mkdir -p "${DIST_DIR}/platforms"
mkdir -p "${DIST_DIR}/imageformats"
mkdir -p "${DIST_DIR}/iconengines"
mkdir -p "${DIST_DIR}/styles"

# Copy generated Windows executables
cp "${BUILD_DIR}/openarch-gui.exe" "${DIST_DIR}/"
cp "${BUILD_DIR}/openarch.exe" "${DIST_DIR}/"
cp "${BUILD_DIR}/openarch-theme-editor.exe" "${DIST_DIR}/"

# 4. Copy Qt Plugins
echo "[4/6] Copying Qt plugins..."
QT_PLUGINS_DIR=""
for candidate in \
    "/usr/x86_64-w64-mingw32/sys-root/mingw/lib/qt5/plugins" \
    "/usr/x86_64-w64-mingw32/sys-root/mingw/plugins" \
    "/usr/x86_64-w64-mingw32/sys-root/mingw/lib64/qt5/plugins"; do
    if [ -d "${candidate}" ]; then
        QT_PLUGINS_DIR="${candidate}"
        break
    fi
done

if [ -n "${QT_PLUGINS_DIR}" ]; then
    echo " -> Found Qt plugins directory: ${QT_PLUGINS_DIR}"
    [ -f "${QT_PLUGINS_DIR}/platforms/qwindows.dll" ] && cp "${QT_PLUGINS_DIR}/platforms/qwindows.dll" "${DIST_DIR}/platforms/"
    [ -f "${QT_PLUGINS_DIR}/imageformats/qsvg.dll" ] && cp "${QT_PLUGINS_DIR}/imageformats/qsvg.dll" "${DIST_DIR}/imageformats/"
    [ -f "${QT_PLUGINS_DIR}/iconengines/qsvgicon.dll" ] && cp "${QT_PLUGINS_DIR}/iconengines/qsvgicon.dll" "${DIST_DIR}/iconengines/"
    [ -f "${QT_PLUGINS_DIR}/styles/qwindowsvistastyle.dll" ] && cp "${QT_PLUGINS_DIR}/styles/qwindowsvistastyle.dll" "${DIST_DIR}/styles/"
else
    echo "Warning: Qt plugins directory not found in standard paths!"
fi

# 5. Resolve and copy all runtime DLL dependencies recursively
echo "[5/6] Resolving and copying all DLL dependencies..."
resolve_dlls() {
    local target_dir="$1"
    local added=1
    
    while [ "$added" -gt 0 ]; do
        added=0
        local needed_dlls
        needed_dlls=$(find "$target_dir" -type f \( -name "*.exe" -o -name "*.dll" \) -exec x86_64-w64-mingw32-objdump -p {} 2>/dev/null \; | grep -i "DLL Name:" | awk '{print $3}' | sort -u)

        for dll in $needed_dlls; do
            # Skip Windows OS native system libraries
            case "$(echo "$dll" | tr '[:upper:]' '[:lower:]')" in
                kernel32.dll|user32.dll|gdi32.dll|shell32.dll|advapi32.dll|ws2_32.dll|ole32.dll|oleaut32.dll|rpcrt4.dll|msvcrt.dll|shlwapi.dll|comctl32.dll|imm32.dll|winmm.dll|comdlg32.dll|dwmapi.dll|uxtheme.dll|setupapi.dll|netapi32.dll|iphlpapi.dll|mpr.dll|version.dll|wsock32.dll|dnsapi.dll|crypt32.dll|secur32.dll|wtsapi32.dll|d3d11.dll|dxgi.dll|opengl32.dll)
                    continue
                    ;;
            esac

            # Check if already present in dist/
            if [ -f "${target_dir}/${dll}" ]; then
                continue
            fi

            # Locate case-insensitively in MinGW sysroot
            local found_path
            found_path=$(find "${SYSROOT_BIN}" -maxdepth 1 -type f -iname "${dll}" | head -n 1)
            if [ -n "${found_path}" ] && [ -f "${found_path}" ]; then
                cp "${found_path}" "${target_dir}/"
                added=$((added + 1))
            fi
        done
    done
}

resolve_dlls "${DIST_DIR}"

# Write qt.conf so Qt loads plugins relative to executable
cat << 'EOF' > "${DIST_DIR}/qt.conf"
[Paths]
Prefix = .
Plugins = .
Libraries = .
EOF

# Copy theme assets
if [ -d "${WORKSPACE}/src/gui/theme/themes" ]; then
    mkdir -p "${DIST_DIR}/themes"
    cp -r "${WORKSPACE}/src/gui/theme/themes/"*.json "${DIST_DIR}/themes/" 2>/dev/null || true
fi
if [ -d "${WORKSPACE}/themes/ui" ]; then
    mkdir -p "${DIST_DIR}/themes/ui"
    cp -r "${WORKSPACE}/themes/ui/"*.json "${DIST_DIR}/themes/ui/" 2>/dev/null || true
fi

# 6. Build NSIS Windows Installer & Portable Archive
echo "[6/6] Compiling NSIS Installer..."
INSTALLER_NAME="OpenArch-Setup-x86_64.exe"
makensis \
    -DDIST_DIR="${DIST_DIR}" \
    -DOUTPUT_INSTALLER="${BUILD_DIR}/${INSTALLER_NAME}" \
    "${WORKSPACE}/docker/installer.nsi"

echo " -> Packaging portable ZIP archive..."
ZIP_NAME="OpenArch-windows-x86_64.zip"
(
    cd "${DIST_DIR}"
    zip -q -r "../${ZIP_NAME}" .
)

echo "=========================================================="
echo " Windows x86_64 Build & Packaging Successful!"
echo " NSIS Installer:  ${BUILD_DIR}/${INSTALLER_NAME}"
echo " Portable Zip:    ${BUILD_DIR}/${ZIP_NAME}"
echo " Binaries & DLLs: ${DIST_DIR}/"
echo "=========================================================="

# Fix host permissions if UID/GID are provided
if [ -n "${HOST_UID:-}" ] && [ -n "${HOST_GID:-}" ]; then
    chown -R "${HOST_UID}:${HOST_GID}" "${BUILD_DIR}" 2>/dev/null || true
fi

