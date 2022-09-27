#include <stdio.h>
#include <syscall.h>
#include <stdlib.h>

int
main (int argc, char **argv)
{
  int arg[4];
  int fibo, max;

  for (int i = 1; i <= 4; i++)
  {
    arg[i] = atoi(argv[i]);
  }

  fibo = fibonacci (arg[1]);
  max = max_of_four_int (arg[1], arg[2], arg[3], arg[4]);

  printf("%d %d\n", fibo, max);

  return EXIT_SUCCESS;
}
