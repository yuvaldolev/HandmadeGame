#!/bin/bash

wildcard="*.h *.cpp *.c *.m *.mm *.inl"

echo "STATICS FOUND:"
grep -r -n -s "static" $wildcard

echo "--------"
echo "--------"

echo "GLOBALS FOUND:"
grep -r -n -s "global_variable" $wildcard

echo "--------"
echo "--------"

echo "LOCAL PERSISTENTS FOUND:"
grep -r -n -s "local_persist" $wildcard

echo "--------"
echo "--------"

echo "INTERNALS FOUND:"
grep -r -n -s "internal" $wildcard