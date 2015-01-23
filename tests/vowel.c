// Adapted from http://www.programmingsimplified.com/c-hello-world-program
#include <stdio.h>
 
int main()
{
  char ch = 'x';
 
  if (ch == 'a' || ch == 'A' || ch == 'e' || ch == 'E' || ch == 'i' || ch == 'I' || ch =='o' || ch=='O' || ch == 'u' || ch == 'U')
    printf("%c is a vowel.\n", ch);
  else
    printf("%c is not a vowel.\n", ch);
 
  return 0;
}
