# using posix standard commands to acquire realpath of file
posix_realpath() {
  if [[ ! $# -eq 1 ]];then
    echo "illegal parameters $@"
    exit 1
  fi
  cd $(dirname $1) 1>/dev/null || exit 1
  local REALPATH_OF_FILE="$(pwd -P)/$(basename $1)"
  cd - 1>/dev/null || exit 1
  echo $REALPATH_OF_FILE
}

lynxtron_envsetup() {
  local SCRIPT_REAL_PATH=$(posix_realpath $1)
  local TOOLS_REAL_PATH=$(dirname $SCRIPT_REAL_PATH)
  export LYNXTRON_ROOT_DIR="$(dirname $TOOLS_REAL_PATH)"
  export BUILDTOOLS_DIR="${LYNXTRON_ROOT_DIR}/buildtools"
  export TOOLSSHARED_DIR="${LYNXTRON_ROOT_DIR}/src/tools_shared"
  export CLANGFORMAT_DIR="${LYNXTRON_ROOT_DIR}/src/tools_shared/buildtools/clang-format"
  export PATH=${CLANGFORMAT_DIR}:${TOOLSSHARED_DIR}:${BUILDTOOLS_DIR}/sccache:${BUILDTOOLS_DIR}/llvm/bin:${BUILDTOOLS_DIR}/gn:${BUILDTOOLS_DIR}/ninja:${BUILDTOOLS_DIR}/node/bin:$PATH
  echo "BUILDTOOLS_DIR: $BUILDTOOLS_DIR"

  # Install repository-managed Git hooks.
  git -C "$LYNXTRON_ROOT_DIR" config core.hooksPath .githooks
}

function python_env_setup() {
  echo "setup python env"
  echo "LYNXTRON_ROOT_DIR: $LYNXTRON_ROOT_DIR"
  VENV_PATH=$LYNXTRON_ROOT_DIR/.venv
  python3 $LYNXTRON_ROOT_DIR/lynxtron_tools/vpython_tools/vpython_env_setup.py --root_dir $LYNXTRON_ROOT_DIR
  source $VENV_PATH/bin/activate
}

lynxtron_envsetup "${BASH_SOURCE:-$0}"
python_env_setup
