#include <stdlib.h>

void
nothing(void)
{
}

int
main(void)
{
  atexit(nothing);
  return 0;
}
