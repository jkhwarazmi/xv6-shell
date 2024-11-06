#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

/* Read a line of characters from stdin. */
int getcmd(char *buf, int nbuf) {
  fprintf(1, ">>> ");
  memset(buf, 0, nbuf);

  char c;
  int bytes = 0;

  while (read(0, &c, 1) > 0) {
    if (++bytes > nbuf) {
      fprintf(2, "command too long\n");
      return 1;
    }

    if (c == '\n') {
      break;
    }

    buf[bytes - 1] = c;
  }

  buf[bytes - 1] = '\0';

  if (buf[0] == '\0') {
    return 1;
  }

  return 0;
}

/*
  A recursive function which parses the command
  at *buf and executes it.
*/
__attribute__((noreturn))
void run_command(char *buf, int nbuf, int *pcp) {
  char *arguments[10];
  int numargs = 0;
  int ws = 1;  // Word start flag
  int we = 0;  // Word end flag
  
  int redirection_left = 0;
  int redirection_right = 0;
  char *file_name_l = 0;
  char *file_name_r = 0;

  int p[2];
  int pipe_cmd = 0;
  char *pipe_right = 0;

  int sequence_cmd = 0;
  char *sequence_right = 0;

  /* Parse the command character by character. */
  for (int i = 0; i < nbuf && buf[i] != '\0'; i++) {
    /* Parse special characters and set flags */
    if (buf[i] == ';') {
      sequence_cmd = 1;
      sequence_right = &buf[i + 1];
      buf[i] = '\0';
      break;
    }
    else if (buf[i] == '|') {
      pipe_cmd = 1;
      pipe_right = &buf[i + 1];
      buf[i] = '\0';
      break;
    }
    else if (buf[i] == '<') {
      redirection_left = 1;
      buf[i] = '\0';
      file_name_l = &buf[i + 1];
      while (file_name_l[0] == ' ') file_name_l++;
      i++;
      continue;
    }
    else if (buf[i] == '>') {
      redirection_right = 1;
      buf[i] = '\0';
      file_name_r = &buf[i + 1];
      while (file_name_r[0] == ' ') file_name_r++;
      i++;
      continue;
    }

    if (!(redirection_left || redirection_right)) {
      /* Parse regular command arguments */
      if (buf[i] == ' ' || buf[i] == '\t') {
        if (!we) continue;
        buf[i] = '\0';
        we = 0;
        ws = 1;
      } else {
        if (ws) {
          arguments[numargs++] = &buf[i];
          ws = 0;
          we = 1;
        }
      }
    } else {
      /* In redirection mode, collecting filename */
      if (buf[i] == ' ' || buf[i] == '\t') {
        buf[i] = '\0';
        break;
      }
    }
  }
  arguments[numargs] = 0;

  /* Handle sequence command */
  if (sequence_cmd) {
    sequence_cmd = 0;
    int pid = fork();
    if (pid != 0) {
      wait(0);
      while (*sequence_right == ' ' || *sequence_right == '\t')
        sequence_right++;
      if (*sequence_right != '\0') // Only run the second command if there is one
        run_command(sequence_right, nbuf - (sequence_right - buf), pcp);
      exit(0);
    }
  }

  /* Handle redirections */
  if (redirection_left) {
    close(0);
    if (open(file_name_l, O_RDONLY) < 0) {
      fprintf(2, "cannot open %s\n", file_name_l);
      exit(1);
    }
  }
  if (redirection_right) {
    close(1);
    if (open(file_name_r, O_WRONLY|O_CREATE|O_TRUNC) < 0) {
      fprintf(2, "cannot open %s\n", file_name_r);
      exit(1);
    }
  }

  /* Execute the command */
  if (numargs == 0) 
    exit(0);

  if (strcmp(arguments[0], "cd") == 0) {
    if (numargs != 2) {
      fprintf(2, "cd requires exactly one argument\n");
      exit(1);
    }
    write(pcp[1], arguments[1], strlen(arguments[1]) + 1);
    exit(2);
  } else {
    if (pipe_cmd) {
      if (pipe(p) < 0) {
        fprintf(2, "pipe failed\n");
        exit(1);
      }

      int pid1 = fork();
      if (pid1 == 0) {
        close(1);
        dup(p[1]);
        close(p[0]);
        close(p[1]);
        if (fork() == 0) {
          exec(arguments[0], arguments);
          fprintf(2, "exec %s failed\n", arguments[0]);
          exit(1);
        }
        wait(0);
        exit(0);
      }

      int pid2 = fork();
      if (pid2 == 0) {
        close(0);
        dup(p[0]);
        close(p[0]);
        close(p[1]);
        while (*pipe_right == ' ' || *pipe_right == '\t')
          pipe_right++;
        run_command(pipe_right, nbuf - (pipe_right - buf), pcp);
      }

      close(p[0]);
      close(p[1]);
      wait(0);
      wait(0);
      exit(0);
    } else {
      if (fork() == 0) {
        exec(arguments[0], arguments);
        fprintf(2, "exec %s failed\n", arguments[0]);
        exit(1);
      }
      wait(0);
      exit(0);
    }
  }
}

int main(void) {
  static char buf[100];
  int pcp[2];

  if (pipe(pcp) < 0) {
    fprintf(2, "pipe failed\n");
    exit(1);
  }

  while(getcmd(buf, sizeof(buf)) >= 0) {
    if(buf[0] == '\0')
      continue;
      
    close(pcp[1]); // Close old write end
    close(pcp[0]); // Close old read end
    
    if (pipe(pcp) < 0) { // Create new pipe
      fprintf(2, "pipe failed\n");
      exit(1);
    }

    if(fork() == 0) {
      close(pcp[0]);
      run_command(buf, sizeof(buf), pcp);
    }

    /* Handle cd command */
    close(pcp[1]);
    int status;
    wait(&status);
    
    if(status == 2) {  // cd command was executed
      char path[100];
      memset(path, 0, sizeof(path));
      if(read(pcp[0], path, sizeof(path)) > 0) {
        if(chdir(path) < 0) {
          fprintf(2, "cannot cd %s\n", path);
        }
      }
    }
  }
  exit(0);
}

/*
BUGS:
- Cannot ls in cd directory (xv6 limitation?)
- > redirection does not work
- No multilevel redirection
- No multilevel pipes (i.e. ignores first pipe)
- ls | grep test = "cannot open test"
- echo y | xargs echo hi = "hi y | xargs echo hi"
- pipes don't work (i.e. pingpong | echo doesn't work)
- semi-colon followed by nothing behaviour? What does Linux do
- echo hello world to temp should not print hello world
- ls; should not print error for ;
- >>> ls;ls>e no filename provided
*/

/*
TESTS:
ls | grep test | cat > myoutput
cat README | grep github
ls | grep test | cat
ls;ls>e
ls;
*/