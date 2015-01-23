// Adapted from http://www.programmingsimplified.com/c-hello-world-program
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
 
int is_palindrome(char*);
void copy_string(char*, char*);
void reverse_string(char*);
int string_length(char*);
int compare_string(char*, char*);
 
int main()
{
   char string[100];
   int result;

   strcpy(string, "amanaplanacanalpanama");
 
   result = is_palindrome(string);
 
   if ( result == 1 )
      printf("\"%s\" is a palindrome string.\n", string);
   else
      printf("\"%s\" is not a palindrome string.\n", string); 
 
   return 0;
}
 
int is_palindrome(char *string)
{
   int check, length;
   char *reverse;
 
   length = string_length(string);    
   reverse = (char*)malloc(length+1);    
 
   copy_string(reverse, string);
   reverse_string(reverse);
 
   check = compare_string(string, reverse);
 
   free(reverse);
 
   if ( check == 0 )
      return 1;
   else
      return 0;
}
 
int string_length(char *string)
{
   int length = 0;  
 
   while(*string)
   {
      length++;
      string++;
   }
 
   return length;
}
 
void copy_string(char *target, char *source)
{
   while(*source)
   {
      *target = *source;
      source++;
      target++;
   }
   *target = '\0';
}
 
void reverse_string(char *string) 
{
   int length, c;
   char *begin, *end, temp;
 
   length = string_length(string);
 
   begin = string;
   end = string;
 
   for ( c = 0 ; c < ( length - 1 ) ; c++ )
       end++;
 
   for ( c = 0 ; c < length/2 ; c++ ) 
   {        
      temp = *end;
      *end = *begin;
      *begin = temp;
 
      begin++;
      end--;
   }
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
