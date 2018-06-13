`include(`foreach.m4')dnl
#include <linux/limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
static char path_buf[PATH_MAX] = {0};
int pwd_no_flto() {
  static char* pwd = NULL;
  if (pwd == NULL)
    pwd = getcwd(path_buf, sizeof(path_buf));
//  fprintf(stderr, "pwd: %s\n", pwd);
foreach(`M4_PATH', ('M4_NO_FLTO`),`dnl
  if (strstr(pwd, "M4_PATH") != NULL) {
//    fprintf(stderr, "strstr\n");
    return 1;
  }
')dnl'
  return 0;
}
int main(int argc, char* argv[]) {
  char** new_argv = malloc((argc+1)*sizeof(char*));
  memset(new_argv, 0, (argc+1)*sizeof(char*));
  char* toolchain_dir = "M4_TOOLCHAIN_DIR";
//  fprintf(stderr, "%s\n", toolchain_dir);
  char the_tool[PATH_MAX];
  size_t ret = snprintf(the_tool,
                        sizeof(the_tool),
                        "%s/M4_TOOL_NAME.real",
                        toolchain_dir);
  char* realpath_tool = realpath(the_tool, NULL);
//  fprintf(stderr, "%s\n", realpath_tool);
  assert( ret < sizeof(the_tool) );
  size_t new_argc = 1;
  for (int i = 1; i < argc; i++) {
   if (pwd_no_flto() && strcmp(argv[i], "-flto") == 0) {
//     fprintf(stderr, "drop -flto\n");
     continue;
   }
   new_argv[new_argc++] = argv[i];
  }
  new_argv[0] = realpath_tool;
  int rc = execv(new_argv[0], new_argv);
  perror("execv");
  assert( rc == 0 );
}
