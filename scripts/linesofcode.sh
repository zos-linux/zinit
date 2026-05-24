#!/bin/sh
find .. -name "*.cpp" -or -name "*.h" | xargs wc
