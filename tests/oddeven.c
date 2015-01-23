// Adapted from http://www.programmingsimplified.com/c-hello-world-program
#include <stdio.h>
 
int main()
{
   int n = 5;
 
   if ( n%2 == 0 )
      printf("%d is even\n", n);
   else
      printf("%d is odd\n", n);
 
   return 0;
}
