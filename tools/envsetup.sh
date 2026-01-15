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
  export PROJECT_ROOT_DIR="$(dirname $LYNXTRON_ROOT_DIR)"
  export BUILDTOOLS_DIR="${PROJECT_ROOT_DIR}/buildtools"
  export TOOLSSHARED_DIR="${LYNXTRON_ROOT_DIR}/tools_shared"
  export CLANGFORMAT_DIR="${LYNXTRON_ROOT_DIR}/tools_shared/buildtools/clang-format"
  export PATH=${CLANGFORMAT_DIR}:${TOOLSSHARED_DIR}:${BUILDTOOLS_DIR}/llvm/bin:${BUILDTOOLS_DIR}/gn:${BUILDTOOLS_DIR}/ninja:$PATH

  # install git hooks
  local GIT_HOOKS_DIR=$(git rev-parse --git-path hooks)
  local HOOKS_DIR=$LYNXTRON_ROOT_DIR/tools/hooks
  local REAL_GIT_HOOKS_DIR=$(posix_realpath $GIT_HOOKS_DIR)
  if [ x$REAL_GIT_HOOKS_DIR != x$HOOKS_DIR ]; then
    if [ -L $GIT_HOOKS_DIR ]; then
      rm -f $GIT_HOOKS_DIR
    elif [ -d $GIT_HOOKS_DIR ]; then
      rm -rf "${GIT_HOOKS_DIR}.bak"
      mv $GIT_HOOKS_DIR "${GIT_HOOKS_DIR}.bak"
    fi
    ln -sf $HOOKS_DIR $GIT_HOOKS_DIR
  fi
}

lynxtron_envsetup "${BASH_SOURCE:-$0}"
