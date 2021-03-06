# Copyright 2017 Istio Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

WD=$(dirname $0)
WD=$(cd $WD; pwd)
ROOT=$(dirname $WD)
WORKSPACE="${ROOT}/WORKSPACE"

# Exit immediately for non zero status
set -e
# Check unset variables
set -u
# Print commands
set -x

GOPATH=/home/prow/go
ROOT=/go/src

# Configure available resources and disable IPv6 tests.
export BAZEL_BUILD_ARGS="--local_ram_resources=12288 --local_cpu_resources=8 --verbose_failures --test_env=ENVOY_IP_TEST_VERSIONS=v4only --test_output=errors"

# Override envoy.
if [[ "${ENVOY_REPOSITORY:-}" && "${ENVOY_PREFIX:-}" ]]; then
  TMP_DIR=$(mktemp -d -t envoy-XXXXXXXXXX)
  trap 'rm -rf ${TMP_DIR:?}' EXIT
  ENVOY_SHA="${ENVOY_SHA:-$(grep -Pom1 "^ENVOY_SHA = \"\K[a-zA-Z0-9]{40}" "$WORKSPACE")}"
  BAZEL_BUILD_ARGS="${BAZEL_BUILD_ARGS} --override_repository=envoy=${TMP_DIR}/${ENVOY_PREFIX}-${ENVOY_SHA}"
  curl -nsSfL "${ENVOY_REPOSITORY}/archive/${ENVOY_SHA}.tar.gz" | tar -C "${TMP_DIR}" -xz
fi

# e2e tests under //test/envoye2e/... use Bazel artifacts.
export BAZEL_OUT="$(bazel info ${BAZEL_BUILD_ARGS} output_path)/k8-fastbuild/bin"

# Use GCP service account when available.
if [[ -n "${GOOGLE_APPLICATION_CREDENTIALS:-}" ]]; then
  echo "Detected GOOGLE_APPLICATION_CREDENTIALS, activating..." >&2
  gcloud auth activate-service-account --key-file="${GOOGLE_APPLICATION_CREDENTIALS}"

  # Use RBE when logged in.
  BAZEL_BUILD_RBE_INSTANCE="${BAZEL_BUILD_RBE_INSTANCE:-projects/istio-testing/instances/default_instance}"
  BAZEL_BUILD_RBE_JOBS="${BAZEL_BUILD_RBE_JOBS:-200}"
  if [[ -n "${BAZEL_BUILD_RBE_INSTANCE}" && "${BAZEL_BUILD_RBE_JOBS}" -gt 0 ]]; then
    echo "Using RBE: ${BAZEL_BUILD_RBE_INSTANCE}"
    export BAZEL_BUILD_ARGS="${BAZEL_BUILD_ARGS} --google_credentials=${GOOGLE_APPLICATION_CREDENTIALS} --config=remote-clang-libc++ --config=remote-ci --remote_instance_name=${BAZEL_BUILD_RBE_INSTANCE} --jobs=${BAZEL_BUILD_RBE_JOBS}"
  fi
fi
