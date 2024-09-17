#pragma once

#ifdef __min
#undef __min
#endif
#define __min(a,b) (((a) < (b)) ? (a) : (b))

#ifdef __max
#undef __max
#endif
#define __max(a,b) (((a) > (b)) ? (a) : (b))
