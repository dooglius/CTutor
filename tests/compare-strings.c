// Adapted from http://www.programmingsimplified.com/c-hello-world-program
#include <stdio.h>
#include <string.h>
 
int compare_string(char*, char*);
 
int main()
{
    char first[100], second[100], result;

    strcpy(first, "Hello world!");
    strcpy(second, "Hello world!!");
 
    result = compare_string(first, second);
 
    if ( result == 0 )
       printf("Both strings are same.\n");
    else
       printf("Entered strings are not equal.\n");
 
    return 0;
}
 
int compare_string(char *first, char *second)
{
   while(*first==*second)
   {
      if ( *first == '\0' || *second == '\0' )
         break;
 
      first++;
      second++;
   }
   if( *first == '\0' && *second == '\0' )
      return 0;
   else
      return -1;
}
