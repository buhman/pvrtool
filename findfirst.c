#include <assert.h>
#include <stddef.h>

#include "findfirst.h"

static_assert((sizeof (long)) >= (sizeof (ptrdiff_t)));

long _findfirst(const char * pattern, _finddata_t * finddata)
{
  int ret = glob(pattern, GLOB_ERR, NULL, &finddata->pglob);
  if (ret == 0) {
    assert(finddata->pglob.gl_pathc >= 1);
    finddata->name = finddata->pglob.gl_pathv[0];
    finddata->ix = 1;
    return (long)finddata;
  } else {
    return -1;
  }
}

int _findnext(long _unused, _finddata_t * finddata)
{
  if (finddata->ix < finddata->pglob.gl_pathc) {
    finddata->name = finddata->pglob.gl_pathv[finddata->ix];
    finddata->ix++;
    return 0;
  } else {
    return -1;
  }
}

void _findclose(long finddata_n)
{
  _finddata_t * finddata = (_finddata_t *)finddata_n;
  globfree(&finddata->pglob);
}
