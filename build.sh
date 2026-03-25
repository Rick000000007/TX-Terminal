#!/bin/bash

# TX Terminal Build Script

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
BUILD_TYPE="Release"
BUILD_TESTS="ON"
BUILD_ANDROID="OFF"
JOBS=$(nproc 2>/dev/null || echo 4)

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

usage() {
    echo "Usage: $0 [options]"
    echo ""
    echo "Options:"
    echo "  -h, --help          Show this help message"
    echo "  -d, --debug         Build debug version"
    echo "  -r, --release       Build release version (default)"
    echo "  -t, --tests         Build tests (default)"
    echo "  -n, --no-tests      Don't build tests"
    echo "  -a, --android       Build for Android"
    echo "  -c, --clean         Clean build directory"
    echo "  -j, --jobs <n>      Number of parallel jobs (default: ${JOBS})"
    echo ""
    echo "Examples:"
    echo "  $0                  Build release with tests"
    echo "  $0 -d -n            Build debug without tests"
    echo "  $0 -a               Build Android APK"
}

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            usage
            exit 0
            ;;
        -d|--debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        -r|--release)
            BUILD_TYPE="Release"
            shift
            ;;
        -t|--tests)
            BUILD_TESTS="ON"
            shift
            ;;
        -n|--no-tests)
            BUILD_TESTS="OFF"
            shift
            ;;
        -a|--android)
            BUILD_ANDROID="ON"
            shift
            ;;
        -c|--clean)
            log_info "Cleaning build directory..."
            rm -rf "${BUILD_DIR}"
            shift
            ;;
        -j|--jobs)
            JOBS="$2"
            shift 2
            ;;
        *)
            log_error "Unknown option: $1"
            usage
            exit 1
            ;;
    esac
done

# Android build
if [[ "${BUILD_ANDROID}" == "ON" ]]; then
    log_info "Building for Android..."
    cd "${SCRIPT_DIR}/android"
    ./gradlew assembleDebug
    log_info "Android build complete!"
    log_info "APK location: app/build/outputs/apk/debug/app-debug.apk"
    exit 0
fi

# Desktop build
log_info "Building TX Terminal (${BUILD_TYPE})..."
log_info "Parallel jobs: ${JOBS}"

# Create build directory
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# Configure
log_info "Configuring with CMake..."
cmake .. \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DTX_BUILD_TESTS="${BUILD_TESTS}" \
    -DTX_BUILD_ANDROID="OFF"

# Build
log_info "Building..."
cmake --build . --parallel "${JOBS}"

# Run tests if enabled
if [[ "${BUILD_TESTS}" == "ON" ]]; then
    log_info "Running tests..."
    ctest --output-on-failure || true
fi

log_info "Build complete!"
log_info "Binary location: ${BUILD_DIR}/tx"

# Print architecture info
if command -v file &> /dev/null; then
    log_info "Binary info:"
    file "${BUILD_DIR}/tx" | sed 's/^/  /'
fi

# Print usage
log_info ""
log_info "To run TX Terminal:"
log_info "  ${BUILD_DIR}/tx"
