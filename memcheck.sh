#!/usr/bin/env bash

set -u

function usage() {
    printf "Usage: %s <program> <lex|parse|validate|tacd|codegen|full>\n" "$0"
}

if [[ $# -ne 2 ]]; then
    echo "error: expected 2 arguments"
    usage
    exit 1
fi

PROGRAM="$1"
shift

STAGE_OPT="$1"
shift

STAGE=()
case "$STAGE_OPT" in
    "lex") STAGE=(--lex);;
    "parse") STAGE=(--parse);;
    "validate") STAGE=(--validate);;
    "tacd") STAGE=(--tacd);;
    "codegen") STAGE=(--codegen);;
    "full") STAGE=();;
    *) usage; exit 1;;
esac

failed=()

test_cases=( $(find tests -type f -name '*.c') )

COLOR_FAIL=$'\033[31m'
COLOR_PASS=$'\033[32m'
COLOR_RESET=$'\033[0m'

current=0
total=${#test_cases[@]}
SECONDS=0
for test_case in "${test_cases[@]}"; do
    ((current++))
    printf "[%*d/%*d]==> testing %s" \
        "${#total}" "$current" \
        "${#total}" "$total" \
        "$test_case"
    valgrind \
        --leak-check=full \
        --show-leak-kinds=all \
        --errors-for-leak-kinds=definite,indirect \
        --error-exitcode=2 \
        -- "$PROGRAM" "${STAGE[@]}" "$test_case" &> /dev/null

    status=$?

    if (( status == 2 )); then
        printf "    %sFAIL%s: memory error/leak detected\n" "$COLOR_FAIL" "$COLOR_RESET"
        failed+=("$test_case")
    else
        printf "    %sPASS%s\n" "$COLOR_PASS" "$COLOR_RESET"
    fi
done
elapsed=$SECONDS

minutes=$(( elapsed / 60 ))
seconds=$(( elapsed % 60 ))

echo
echo "=================================================="
echo " Memory Check Results"
echo "=================================================="
printf "program: %s\nstage: %s\n\n" "$PROGRAM" "$STAGE_OPT"
printf "Total tests: %d\n" "$total"
printf "Elapsed time: %dm %ds (%ds)\n" "$minutes" "$seconds" "$elapsed"
printf "Passed:      %s%d%s\n" "$COLOR_PASS" "$(( $total - ${#failed[@]} ))" "$COLOR_RESET"
printf "Failed:      %s%d%s\n" "$COLOR_FAIL" "${#failed[@]}" "$COLOR_RESET"


if (( ${#failed[@]} > 0 )); then
    fail_count=0
    echo
    echo "Tests with memory errors/leaks:"
    for test_case in "${failed[@]}"; do
        ((fail_count++))
        printf "[%s%d%s]    %s%s%s\n" "$COLOR_FAIL" "$fail_count" "$COLOR_RESET" "$COLOR_FAIL" "$test_case" "$COLOR_RESET"
    done
fi

exit "${#failed[@]}"
