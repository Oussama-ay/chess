
#!/usr/bin/env bash

# Re-exec with bash if the script was started under /bin/sh or another shell.
if [ -z "${BASH_VERSION:-}" ]; then
	if command -v bash >/dev/null 2>&1; then
		exec bash "$0" "$@"
	else
		echo "This script requires bash. Run it with 'bash ./engine/docker-build-wasm.sh'" >&2
		exit 1
	fi
fi

set -euo pipefail

# Robust Docker build wrapper for Emscripten. Tries several strategies to
# avoid the "cannot setuid to unmapped uid" user-namespace error.
# Usage: run this from the repo root: ./engine/docker-build-wasm.sh

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="${SCRIPT_DIR%/engine}"
DOCKER_IMAGE="emscripten/emsdk"

COMMON_MOUNTS=("-v" "${REPO_ROOT}":/src "-w" /src/engine "$DOCKER_IMAGE" emmake make)

try_cmd() {
	echo
	echo "==> $*"
	set +e
	"$@"
	local rc=$?
	set -e
	return $rc
}


echo "Building engine with Emscripten Docker image: ${DOCKER_IMAGE}"

# 1) Preferred: map host UID:GID so generated files are owned by the host user
USER_ARG=("-u" "$(id -u):$(id -g)")
if try_cmd docker run --rm "${USER_ARG[@]}" "${COMMON_MOUNTS[@]}"; then
	echo "Build succeeded with host UID mapping.";
	exit 0
fi

echo "UID mapping failed — retrying with --userns=host (avoid user namespace mapping)."
if try_cmd docker run --rm --userns=host "${COMMON_MOUNTS[@]}"; then
	echo "Build succeeded with --userns=host.";
	exit 0
fi

echo "Retrying without user mapping (container runs as root)."
if try_cmd docker run --rm "${COMMON_MOUNTS[@]}"; then
	echo "Build succeeded running container as root.";
	exit 0
fi

echo "All docker run attempts failed. See output above for details."
exit 1
