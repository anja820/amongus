#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
make
if ! command -v valgrind >/dev/null 2>&1; then
  echo "valgrind is not installed. On Ubuntu/Debian: sudo apt install valgrind"
  exit 1
fi
cat > /tmp/amongos_bug_demo.c <<'C'
#include <stdlib.h>
#include <string.h>
int main(void) {
    char *name = malloc(5);
    strcpy(name, "crewmate"); // overflow: destination too small
    char *leak = malloc(1024); // leak: never freed
    (void)leak;
    free(name);
    return 0;
}
C
gcc -g -O0 -o /tmp/amongos_bug_demo /tmp/amongos_bug_demo.c
valgrind --leak-check=full --track-origins=yes /tmp/amongos_bug_demo
