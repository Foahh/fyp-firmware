#!/bin/bash

find ./Appli ./FSBL \( -name "*.c" -o -name "*.h" -o -name "*.cpp" -o -name "*.hpp" \) \
  -not -path "*/build/*" \
  -not -path "*/.cache/*" \
  -not -path "*/Proto/nanopb/*" \
  -print | xargs clang-format -i
