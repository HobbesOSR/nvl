#include <stdio.h>
#include <unistd.h>



int
main ()
{

  open ("/dev/kgni0", 0666);
  printf ("Hello, World!\n");

  return 0;
}
