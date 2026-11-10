#!/usr/bin/env bash
#
# Memory-safety and static-analysis checks for miru.
#
# Single entry point shared by:
#   - Grimoire        (grim cast sanitize, grim cast asan, ...)
#   - .githooks/pre-commit
#   - .woodpecker/sanitizer.yml
#
# Every stage builds into its own directory (build-asan/, build-analyzer/,
# build-valgrind/), so the regular build/ directory, which clang-tidy and the
# release pipeline use, is never touched.
#
# Usage: scripts/sanitize.sh [stage]
#
#   asan       build with ASan + UBSan and run the smoke tests
#   analyzer   build with GCC -fanalyzer and fail on any analyzer warning
#   valgrind   build without sanitizers and run the smoke tests in Memcheck
#   check      asan + analyzer (default; what the hook and CI enforce)
#   all        check + valgrind (valgrind is skipped if it is not installed)
#   clean      remove build-asan/, build-analyzer/ and build-valgrind/
#
# Environment:
#   CC                 compiler for asan/valgrind              (default: gcc)
#   ANALYZER_CC        GCC used for -fanalyzer (>= 13)         (default: gcc)
#   ANALYZER_MIN_GCC   lowest GCC major version accepted       (default: 13)
#   JOBS               parallel build jobs                     (default: nproc)
#   WAYLAND_PROTOCOLS_DIR  forwarded to CMake when set
#   MIRU_ANALYZER_FLAGS    extra flags for the analyzer build,
#                          e.g. "-Wno-analyzer-possible-null-argument"
#   MIRU_ANALYZER_STRICT=1 also enable the checks that are off by default
#                          because of known false positives (see below)
#   MIRU_VERBOSE=1     stream build output instead of showing it on failure
#   ASAN_OPTIONS / UBSAN_OPTIONS  appended to the defaults used here
#
# Exit status is non-zero if any stage reports a genuine problem.

set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT" || exit 1

CC="${CC:-gcc}"
ANALYZER_CC="${ANALYZER_CC:-gcc}"
ANALYZER_MIN_GCC="${ANALYZER_MIN_GCC:-13}"
# Analyzer checks that are known to give false positives on miru with GCC 13/14:
#   fd-leak                      fd stored in a struct member (ipc_server.c)
#   out-of-bounds                indexing a static string table (annotations.c)
#   use-of-uninitialized-value   memcpy of a struct with padding (keybinds.c)
# Everything else (malloc-leak, double-free, use-after-free, null-dereference,
# ...) stays enabled. MIRU_ANALYZER_STRICT=1 turns all of them back on.
ANALYZER_DEFAULT_FLAGS="-Wno-analyzer-fd-leak -Wno-analyzer-out-of-bounds -Wno-analyzer-use-of-uninitialized-value"
[ "${MIRU_ANALYZER_STRICT:-0}" = 1 ] && ANALYZER_DEFAULT_FLAGS=""
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 2)}"
VERBOSE="${MIRU_VERBOSE:-0}"

# Exit code used by ASan, UBSan and Valgrind when they find a problem. Chosen
# so it cannot be confused with miru's own exit codes (0, 1 and 255).
SANITIZER_RC=99

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

log() { printf '==> %s\n' "$*"; }
warn() { printf 'warning: %s\n' "$*" >&2; }
err() { printf 'error: %s\n' "$*" >&2; }

need() {
  if ! command -v "$1" >/dev/null 2>&1; then
    err "'$1' not found. ${2:-Install it and try again.}"
    return 1
  fi
}

# run_logged <label> <cmd...>
# Quiet on success; dumps the tail of the output on failure.
run_logged() {
  local label=$1 file rc
  shift
  file="$TMP/$(echo "$label" | tr -c 'A-Za-z0-9' '_').log"
  if [ "$VERBOSE" = 1 ]; then
    "$@" 2>&1 | tee "$file"
    rc=${PIPESTATUS[0]}
  else
    "$@" >"$file" 2>&1
    rc=$?
  fi
  LAST_LOG=$file
  if [ "$rc" -ne 0 ]; then
    err "$label failed (exit $rc). Last output:"
    tail -n 60 "$file" >&2
  fi
  return "$rc"
}

# configure <build-dir> <compiler> <cflags> <ldflags>
configure() {
  local dir=$1 cc=$2 cflags=$3 ldflags=$4
  local -a args=(
    -S . -B "$dir" -G "Unix Makefiles"
    -DCMAKE_BUILD_TYPE=Debug
    -DCMAKE_C_COMPILER="$cc"
    -DCMAKE_C_FLAGS="$cflags"
    -DCMAKE_EXE_LINKER_FLAGS="$ldflags"
  )
  if [ -n "${WAYLAND_PROTOCOLS_DIR:-}" ]; then
    args+=(-DWAYLAND_PROTOCOLS_DIR="$WAYLAND_PROTOCOLS_DIR")
  fi
  # CMake does not reliably honour a new compiler in an existing build tree
  # and can end up half-configured, so start over when it changes.
  if [ -f "$dir/CMakeCache.txt" ] &&
    [ "$(sed -n 's/^CMAKE_C_COMPILER:[A-Z]*=//p' "$dir/CMakeCache.txt")" != "$cc" ]; then
    rm -rf "$dir"
  fi
  run_logged "cmake configure ($dir)" cmake "${args[@]}"
}

# ---------------------------------------------------------------------------
# Smoke tests
#
# miru has no unit tests yet and the daemon needs a Wayland compositor, so
# these run everything that works without one: argument handling, config
# creation and parsing (including malformed files) and the failure paths that
# run when no compositor or daemon is reachable. CTest tests, once added to
# CMakeLists.txt, are run after these (ASan stage only).
#
# A case fails when the sanitizer/valgrind exit code is returned, when the
# process times out or dies from a signal, or when a sanitizer report shows
# up in the output. Ordinary non-zero exits (1, 255) are expected for the
# "no compositor" and "no daemon" cases and are accepted.
# ---------------------------------------------------------------------------

case_failed() {
  local rc=$1 out=$2
  [ "$rc" -eq "$SANITIZER_RC" ] && return 0
  [ "$rc" -eq 124 ] && return 0                      # timeout
  [ "$rc" -ge 128 ] && [ "$rc" -ne 255 ] && return 0 # SIGSEGV, SIGABRT, ...
  printf '%s' "$out" |
    grep -Eq 'ERROR: (Address|Leak)Sanitizer|runtime error:|ERROR SUMMARY: [1-9]' &&
    return 0
  return 1
}

# case_run <label> <cmd...>   (uses WRAP and CASE_TIMEOUT, runs in the sandbox)
case_run() {
  local label=$1 out rc
  shift
  out=$(
    env -u WAYLAND_SOCKET -u DISPLAY "${SANDBOX_ENV[@]}" \
      timeout --kill-after=5 "$CASE_TIMEOUT" \
      ${WRAP[@]+"${WRAP[@]}"} "$@" 2>&1
  )
  rc=$?
  if case_failed "$rc" "$out"; then
    err "smoke test failed: $label (exit $rc)"
    printf '%s\n' "$out" | tail -n 80 >&2
    return 1
  fi
  CASES_RUN=$((CASES_RUN + 1))
  return 0
}

# smoke_suite <build-dir> <per-case-timeout> [wrapper cmd...]
smoke_suite() {
  local bindir=$1 sandbox cfg failed=0
  CASE_TIMEOUT=$2
  shift 2
  WRAP=("$@")
  CASES_RUN=0

  local daemon="$ROOT/$bindir/miru-daemon" ctl="$ROOT/$bindir/miructl"
  [ -x "$daemon" ] && [ -x "$ctl" ] || {
    err "missing binaries in $bindir"
    return 1
  }

  sandbox=$(mktemp -d -p "$TMP")
  mkdir -p "$sandbox/run" "$sandbox/cfg/miru" "$sandbox/home"
  chmod 700 "$sandbox/run"
  cfg="$sandbox/cfg/miru/config.toml"
  SANDBOX_ENV=(
    XDG_RUNTIME_DIR="$sandbox/run"
    XDG_CONFIG_HOME="$sandbox/cfg"
    HOME="$sandbox/home"
    WAYLAND_DISPLAY=miru-sanitize-no-compositor
  )

  # Argument handling
  case_run "miru-daemon --help" "$daemon" --help || failed=1
  case_run "miru-daemon --version" "$daemon" --version || failed=1
  case_run "miructl --help" "$ctl" --help || failed=1

  # IPC client with no daemon listening, and with bad arguments
  case_run "miructl toggle (no daemon)" "$ctl" toggle || failed=1
  case_run "miructl bogus command" "$ctl" definitely-not-a-command || failed=1
  case_run "miructl without arguments" "$ctl" || failed=1

  # Daemon start-up: config creation + load, config watch, then compositor
  # connection failure and cleanup. First run creates the default config, the
  # second run parses it.
  rm -f "$cfg"
  case_run "miru-daemon, default config created" "$daemon" || failed=1
  case_run "miru-daemon, default config reloaded" "$daemon" || failed=1

  # Malformed configs must be rejected without memory errors
  local name
  for name in empty unterminated bad-values long-line long-header; do
    case "$name" in
    empty) : >"$cfg" ;;
    unterminated) printf '[zoom\nfactor = \n[[' >"$cfg" ;;
    bad-values)
      printf '[zoom]\nfactor = -1\nmax_factor = 0\nincrement = nan\n' >"$cfg"
      printf '[spotlight]\nradius = 99999999999999999999\ndim = 1e999\n' >>"$cfg"
      printf '[annotate]\ndefault = "%s"\n' "$(printf 'x%.0s' {1..300})" >>"$cfg"
      ;;
    long-line) head -c 8192 /dev/zero | tr '\0' 'A' >"$cfg" ;;
    long-header) head -c 4096 /dev/zero | tr '\0' '[' >"$cfg" ;;
    esac
    case_run "miru-daemon, malformed config ($name)" "$daemon" || failed=1
  done

  [ "$failed" -eq 0 ] && log "$CASES_RUN smoke tests passed"
  return "$failed"
}

# ---------------------------------------------------------------------------
# Stages
# ---------------------------------------------------------------------------

stage_asan() {
  local dir=build-asan
  local san="address,undefined"
  need cmake && need "$CC" && need pkg-config || return 1

  log "ASan + UBSan: configuring and building in $dir/ (CC=$CC)"
  configure "$dir" "$CC" \
    "-O1 -g -fno-omit-frame-pointer -fsanitize=$san -fno-sanitize-recover=all" \
    "-fsanitize=$san" || return 1
  run_logged "build ($dir)" cmake --build "$dir" -j"$JOBS" || return 1

  # Later options win, so anything the caller exports overrides the defaults.
  export ASAN_OPTIONS="exitcode=$SANITIZER_RC:detect_leaks=1:detect_stack_use_after_return=1:strict_string_checks=1:check_initialization_order=1${ASAN_OPTIONS:+:$ASAN_OPTIONS}"
  export UBSAN_OPTIONS="exitcode=$SANITIZER_RC:halt_on_error=1:print_stacktrace=1${UBSAN_OPTIONS:+:$UBSAN_OPTIONS}"

  log "ASan + UBSan: running smoke tests"
  smoke_suite "$dir" 60 || return 1

  # A real test suite, once CMakeLists.txt defines one
  if [ -f "$dir/CTestTestfile.cmake" ]; then
    log "ASan + UBSan: running ctest"
    run_logged "ctest" ctest --test-dir "$dir" --output-on-failure || return 1
  fi
}

stage_analyzer() {
  local dir=build-analyzer cc=$ANALYZER_CC findings
  need cmake && need "$cc" && need pkg-config || return 1

  # -fanalyzer is GCC only (clang has its own, different analyzer)
  if "$cc" -dM -E -x c /dev/null 2>/dev/null | grep -q __clang__ ||
    ! echo 'int main(void) { return 0; }' |
    "$cc" -fanalyzer -x c -c -o /dev/null - 2>/dev/null; then
    err "'$cc' does not support -fanalyzer. Install GCC >= $ANALYZER_MIN_GCC or set ANALYZER_CC."
    return 1
  fi
  if [ "$("$cc" -dumpversion | cut -d. -f1)" -lt "$ANALYZER_MIN_GCC" ]; then
    err "'$cc' is GCC $("$cc" -dumpfullversion); the analyzer in older releases is too noisy on C."
    err "Use GCC >= $ANALYZER_MIN_GCC (e.g. ANALYZER_CC=gcc-14) or lower ANALYZER_MIN_GCC."
    return 1
  fi

  log "GCC analyzer: configuring and building in $dir/ ($("$cc" -dumpfullversion))"
  configure "$dir" "$cc" \
    "-O0 -g -fanalyzer $ANALYZER_DEFAULT_FLAGS ${MIRU_ANALYZER_FLAGS:-}" "" || return 1

  # Analyzer findings are warnings and do not fail the compile, and an
  # up-to-date object file would not report them again. Always rebuild
  # from scratch so a second run cannot pass by accident.
  run_logged "build ($dir)" cmake --build "$dir" --clean-first -j"$JOBS" || return 1

  # Ignore generated wayland-scanner code in the build dir.
  findings=$(grep -E ': warning: .*\[-Wanalyzer-' "$LAST_LOG" | grep -v "/$dir/" || true)
  if [ -n "$findings" ]; then
    err "GCC analyzer reported $(printf '%s\n' "$findings" | wc -l) problem(s):"
    # Show the complete diagnostics (event paths included), not only the headlines.
    grep -v '^\[ *[0-9]*%\]' "$LAST_LOG" | grep -vE '^(gmake|make)' >&2
    return 1
  fi
  log "GCC analyzer: no findings"
}

stage_valgrind() {
  local dir=build-valgrind
  if ! command -v valgrind >/dev/null 2>&1; then
    if [ "${VALGRIND_OPTIONAL:-0}" = 1 ]; then
      warn "valgrind not installed, skipping"
      return 0
    fi
    err "'valgrind' not found. Install it (e.g. pacman -S valgrind) and try again."
    return 1
  fi
  need cmake && need "$CC" && need pkg-config || return 1

  # Valgrind cannot run on top of ASan, so this is a plain debug build.
  log "Valgrind: configuring and building in $dir/ (CC=$CC)"
  configure "$dir" "$CC" "-O0 -g -fno-omit-frame-pointer" "" || return 1
  run_logged "build ($dir)" cmake --build "$dir" -j"$JOBS" || return 1

  log "Valgrind: running smoke tests"
  smoke_suite "$dir" 300 \
    valgrind --quiet --error-exitcode="$SANITIZER_RC" --leak-check=full \
    --errors-for-leak-kinds=definite,indirect \
    --show-leak-kinds=definite,indirect --track-origins=yes --num-callers=30
}

stage_clean() {
  log "Removing build-asan/ build-analyzer/ build-valgrind/"
  rm -rf build-asan build-analyzer build-valgrind
}

usage() {
  sed -n '2,/^$/{s/^# \{0,1\}//;p}' "${BASH_SOURCE[0]}" | sed '$d'
}

main() {
  local cmd=${1:-check} stage
  local -a stages=() passed=() failed=()

  case "$cmd" in
  asan) stages=(asan) ;;
  analyzer | analyze) stages=(analyzer) ;;
  valgrind) stages=(valgrind) ;;
  check) stages=(asan analyzer) ;;
  all)
    stages=(asan analyzer valgrind)
    VALGRIND_OPTIONAL=1
    ;;
  clean) stages=(clean) ;;
  -h | --help | help)
    usage
    return 0
    ;;
  *)
    err "unknown stage '$cmd'"
    usage >&2
    return 2
    ;;
  esac

  for stage in "${stages[@]}"; do
    if "stage_$stage"; then
      passed+=("$stage")
    else
      failed+=("$stage")
    fi
  done

  [ "$cmd" = clean ] && return 0
  echo
  [ ${#passed[@]} -gt 0 ] && log "passed: ${passed[*]}"
  if [ ${#failed[@]} -gt 0 ]; then
    err "failed: ${failed[*]}"
    return 1
  fi
  return 0
}

main "$@"
