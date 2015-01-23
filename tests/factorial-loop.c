// Adapted from http://www.programmingsimplified.com/c-hello-world-program
#include <stdio.h>
 
int main()
{
  int c, n = 7, fact = 1;
 
  for (c = 1; c <= n; c++)
    fact = fact * c;
 
  printf("Factorial of %d = %d\n", n, fact);
 
  return 0;
}
