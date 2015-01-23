// Adapted from http://www.programmingsimplified.com/c-hello-world-program
#include <stdio.h>
 
int main()
{
  int year = 1996;
 
  if ( year%400 == 0)
    printf("%d is a leap year.\n", year);
  else if ( year%100 == 0)
    printf("%d is not a leap year.\n", year);
  else if ( year%4 == 0 )
    printf("%d is a leap year.\n", year);
  else
    printf("%d is not a leap year.\n", year);  
 
  return 0;
}
