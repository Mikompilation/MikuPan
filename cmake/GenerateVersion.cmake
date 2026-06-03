# Runs `git` against GIT_DIR to resolve the latest tag, commit hash, branch and
# `git describe`, then configures SRC -> DST. Run both at configure time and as a
# build-time custom target so the version header tracks the actual built commit.
#
# Required -D args: SRC (template), DST (output header), GIT_DIR (repo root).

set(MIKUPAN_GIT_TAG      "unknown")
set(MIKUPAN_GIT_COMMIT   "unknown")
set(MIKUPAN_GIT_DESCRIBE "unknown")
set(MIKUPAN_GIT_BRANCH   "unknown")

find_program(GIT_EXE git)

if(GIT_EXE)
    # Latest tag reachable from HEAD (empty if the repo has no tags).
    execute_process(
        COMMAND "${GIT_EXE}" -C "${GIT_DIR}" describe --tags --abbrev=0
        OUTPUT_VARIABLE _tag OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET RESULT_VARIABLE _tag_res)
    if(_tag_res EQUAL 0 AND _tag)
        set(MIKUPAN_GIT_TAG "${_tag}")
    endif()

    # Short commit hash.
    execute_process(
        COMMAND "${GIT_EXE}" -C "${GIT_DIR}" rev-parse --short HEAD
        OUTPUT_VARIABLE _commit OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET RESULT_VARIABLE _commit_res)
    if(_commit_res EQUAL 0 AND _commit)
        set(MIKUPAN_GIT_COMMIT "${_commit}")
    endif()

    # tag-distance-hash, with a -dirty suffix when the tree has local changes.
    execute_process(
        COMMAND "${GIT_EXE}" -C "${GIT_DIR}" describe --tags --always --dirty
        OUTPUT_VARIABLE _desc OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET RESULT_VARIABLE _desc_res)
    if(_desc_res EQUAL 0 AND _desc)
        set(MIKUPAN_GIT_DESCRIBE "${_desc}")
    endif()

    # Current branch.
    execute_process(
        COMMAND "${GIT_EXE}" -C "${GIT_DIR}" rev-parse --abbrev-ref HEAD
        OUTPUT_VARIABLE _branch OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET RESULT_VARIABLE _branch_res)
    if(_branch_res EQUAL 0 AND _branch)
        set(MIKUPAN_GIT_BRANCH "${_branch}")
    endif()
endif()

# Date only (no time): keeps the header content stable across rebuilds on the
# same day, so configure_file (below) won't needlessly recompile the UI unless
# the git state actually changes.
string(TIMESTAMP MIKUPAN_BUILD_DATE "%Y-%m-%d")

# configure_file skips the write when the content is unchanged, so an unchanged
# git state won't force the dependent translation unit to recompile.
configure_file("${SRC}" "${DST}" @ONLY)
