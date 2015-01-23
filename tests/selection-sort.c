// Adapted from http://www.programmingsimplified.com/c-hello-world-program
#include <stdio.h>
#include <string.h>
 
int main()
{
   int array[10], n = 10, c, d, position, swap;

   array[0] = 10;
   array[1] = 29;
   array[2] = 7;
   array[3] = 4;
   array[4] = 6;
   array[5] = 9;
   array[6] = 19;
   array[7] = 29;
   array[8] = 103;
   array[9] = 3;
 
   for ( c = 0 ; c < ( n - 1 ) ; c++ )
   {
      position = c;
 
      for ( d = c + 1 ; d < n ; d++ )
      {
         if ( array[position] > array[d] )
            position = d;
      }
      if ( position != c )
      {
         swap = array[c];
         array[c] = array[position];
         array[position] = swap;
      }
   }
 
   printf("Sorted list in ascending order:\n");
 
   for ( c = 0 ; c < n ; c++ )
      printf("%d\n", array[c]);
 
   return 0;
}
