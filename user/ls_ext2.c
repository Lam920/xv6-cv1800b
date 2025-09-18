#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/include/vfs.h"
#include "kernel/include/ext2.h"

char*
fmtname(char *path)
{
  static char buf[EXT2_NAME_LEN + 1];
  char *p;

  // Find first character after last slash.
  for (p = path + strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // Return blank-padded name.
  if (strlen(p) >= EXT2_NAME_LEN)
    return p;
  memmove(buf, p, strlen(p));
  memset(buf + strlen(p), ' ', EXT2_NAME_LEN - strlen(p));
  buf[EXT2_NAME_LEN] = '\0';

  return buf;
}

void
ls(char *path)
{
  char buf[512], *p;
  int fd;
  struct ext2_dir_entry_2 de;
  struct stat st;

  if ((fd = open(path, 0)) < 0){
    fprintf(2, "ls: cannot open %s\n", path);
    return;
  }

  if (fstat(fd, &st) < 0){
    fprintf(2, "ls: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch (st.type){
  case T_FILE:
    printf("%s %d %d %d\n", fmtname(path), st.type, st.ino, st.size);
    break;

  case T_DIR:
    if (strlen(path) + 1 + EXT2_NAME_LEN + 1 > sizeof buf) {
      printf("ls: path too long\n");
      break;
    }
    strcpy(buf, path);
    p = buf + strlen(buf);
    *p++ = '/';
    
    // Read directory entries
    while (read(fd, &de, sizeof(struct ext2_dir_entry_2)) > 0) {
      // Read the name separately
      char name_buf[EXT2_NAME_LEN + 1];
      read(fd, name_buf, de.name_len);
      name_buf[de.name_len] = '\0';
      
      // Skip padding to next entry
      int padding = de.rec_len - (sizeof(struct ext2_dir_entry_2) + de.name_len);
      if (padding > 0) {
        char dummy;
        for (int i = 0; i < padding; i++) {
          read(fd, &dummy, 1);
        }
      }

      if (de.inode == 0)
        continue;

      // Build full path
      if (de.name_len >= sizeof(buf) - (p - buf)) {
        printf("ls: name too long\n");
        continue;
      }
      memmove(p, name_buf, de.name_len);
      p[de.name_len] = '\0';

      if (stat(buf, &st) < 0){
        printf("ls: cannot stat %s\n", buf);
        continue;
      }
      printf("%s %d %d %d\n", fmtname(name_buf), st.type, st.ino, st.size);
    }
    break;
  }
  close(fd);
}

int
main(int argc, char *argv[])
{
  int i;

  if(argc < 2){
    ls(".");
    return 0;
  }
  for(i=1; i<argc; i++)
    ls(argv[i]);
  return 0;
}