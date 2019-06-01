@echo off

set wildcard=*.h *.cpp *.c *.inl

echo STATICS FOUND:
findstr -s -n -i -l "static" %wildcard%

echo --------
echo --------

echo GLOBALS FOUND:
findstr -s -n -i -l "global_variable" %wildcard%

echo --------
echo --------

echo LOCAL PERSISTENTS FOUND:
findstr -s -n -i -l "local_persist" %wildcard%

echo --------
echo --------

echo INTERNALS FOUND:
findstr -s -n -i -l "internal" %wildcard%