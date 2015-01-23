// Adapted from http://www.programmingsimplified.com/c-hello-world-program
#include <stdio.h>
#include <string.h>
 
void concatenate_string(char*, char*);
 
int main()
{
    char original[100], add[100];

    strcpy(original, "hello");
    strcpy(add, " world!!!");
 
    concatenate_string(original, add);
 
    printf("String after concatenation is \"%s\"\n", original);
 
    return 0;
}
 
void concatenate_string(char *original, char *add)
{
   while(*original)
      original++;
 
   while(*add)
   {
      *original = *add;
      add++;
      original++;
   }
   *original = '\0';
}
