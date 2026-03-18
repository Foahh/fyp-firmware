#!/bin/bash

find . -path ./Libraries -prune -o \( -name "*.c" -o -name "*.h" \) -print | xargs clang-format -i