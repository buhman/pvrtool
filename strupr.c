#include <string.h>

#include "strupr.h"

static char upcase(const char c)
{
  switch (c) {
  case 'a': return 'A';
  case 'b': return 'B';
  case 'c': return 'C';
  case 'd': return 'D';
  case 'e': return 'E';
  case 'f': return 'F';
  case 'g': return 'G';
  case 'h': return 'H';
  case 'i': return 'I';
  case 'j': return 'J';
  case 'k': return 'K';
  case 'l': return 'L';
  case 'm': return 'M';
  case 'n': return 'N';
  case 'o': return 'O';
  case 'p': return 'P';
  case 'q': return 'Q';
  case 'r': return 'R';
  case 's': return 'S';
  case 't': return 'T';
  case 'u': return 'U';
  case 'v': return 'V';
  case 'w': return 'W';
  case 'x': return 'X';
  case 'y': return 'Y';
  case 'z': return 'Z';
  default: return c;
  }
}

void strupr(char * s)
{
  for (size_t i = 0; i < strlen(s); i++) {
    s[i] = upcase(s[i]);
  }
}
