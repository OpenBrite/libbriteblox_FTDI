/* main.c

   Example for briteblox_new()

   This program is distributed under the GPL, version 2
*/

#include <stdio.h>
#include <stdlib.h>
#include <briteblox.h>

int main(void)
{
  struct briteblox_context *briteblox;
  int retval = EXIT_SUCCESS;

  if ((briteblox = briteblox_new()) == 0)
  {
    fprintf(stderr, "briteblox_new failed\n");
    return EXIT_FAILURE;
  }
  
  return retval;
}
