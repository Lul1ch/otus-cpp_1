#!/usr/bin/env bash
# Скачивает готовую библиотеку инференса CatBoost (libcatboostmodel.so)
# и заголовок C API в third_party/catboost.
set -euo pipefail

VERSION="${CATBOOST_VERSION:-1.2.10}"
ARCH="$(uname -m)"   # x86_64 или aarch64
DEST="$(cd "$(dirname "$0")/.." && pwd)/third_party/catboost"

mkdir -p "${DEST}"
echo "Downloading CatBoost ${VERSION} (${ARCH}) to ${DEST}"

curl -fL -o "${DEST}/libcatboostmodel.so" \
    "https://github.com/catboost/catboost/releases/download/v${VERSION}/libcatboostmodel-linux-${ARCH}-${VERSION}.so"
curl -fL -o "${DEST}/c_api.h" \
    "https://raw.githubusercontent.com/catboost/catboost/v${VERSION}/catboost/libs/model_interface/c_api.h"

echo "Done"
