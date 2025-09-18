#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{

  if(argc < 4){
    fprintf(2, "Usage: dev num, path, fs-type...\n");
    return -1;
  }

  if(mount(argv[1], argv[2], argv[3]) < 0){
    fprintf(2, "mount: failed to mounting device\n");
  }

  return -1;
}

