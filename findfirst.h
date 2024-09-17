#pragma once

#include <glob.h>

typedef struct _finddata {
  char * name;
  glob_t pglob;
  size_t ix;
} _finddata_t;

long _findfirst(const char * pattern, _finddata_t * finddata);
int _findnext(long _unused, _finddata_t * finddata);
void _findclose(long finddata_n);
