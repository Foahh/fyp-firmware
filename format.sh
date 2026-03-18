#!/bin/bash

find ./Appli ./FSBL \( -name "*.c" -o -name "*.h" -o -name "*.cpp" -o -name "*.hpp" \) \
  -not -path "*/build/*" \
  -not -path "*/.cache/*" \
  -print | xargs clang-format -i
