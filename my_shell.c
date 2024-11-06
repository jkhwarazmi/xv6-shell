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
    fprintf(2, "no command\n");
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

  /* Useful data structures and flags. */
  char *arguments[10];
  char arg_strings[10][nbuf];
  int numargs = 0;
  int len = 0;
  /* Word start/end */
  int ws = 1;
  int we = 0;

  int redirection_left = 0;
  int redirection_right = 0;
  char *file_name_l = 0;
  char *file_name_r = 0;
  char file_name[nbuf];

  int p[2];
  int pipe_cmd = 0;

  int sequence_cmd = 0;

  int i = 0;
  /* Parse the command character by character. */
  for (; i < nbuf && buf[i] != '\0'; i++) {
    /* Parse the current character and set-up various flags:
       sequence_cmd, redirection, pipe_cmd and similar. */

    switch(buf[i]) {
      case ';':
        sequence_cmd = i + 1;
        break;
      case '|':
        pipe_cmd = i + 1;
        break;
      case '<':
        redirection_left = 1;
        break;
      case '>':
        redirection_right = 1;
        break;
    }

    // Can't start with one of these
    if (buf[i] == '|' || buf[i] == ';' || buf[i] == '<' || buf[i] == '>') {
      if (numargs == 0) {
        buf[i + 1] = '\0';
        fprintf(2, "command cannot start with %s\n", &buf[i]);
        exit(1);
      }

      // Allow no spaces before or after
      if (we) {
        we = 0;
        ws = 1;
        arg_strings[numargs - 1][len] = '\0';
        len = 0;
      }
      continue;
    }

    if (!(redirection_left || redirection_right)) {
      /* No redirection, continue parsing command. */

      if (buf[i] == ' ' || buf[i] == '\t') {
        if (ws) {
          continue;
        } else if (we) {
          we = 0;
          ws = 1;
          arg_strings[numargs - 1][len] = '\0';
          len = 0;
        }
      } else {
        if (ws) {
          ws = 0;
          we = 1;
          numargs++;
        }

        if (numargs > 10) {
          fprintf(2, "too many arguments\n");
          exit(1);
        }

        if (len >= nbuf) {
          fprintf(2, "argument too long\n");
          exit(1);
        }

        arg_strings[numargs - 1][len] = buf[i];
        len++;
      }
    } else {
      /* Redirection command. Capture the file names. */
      i++;

      // Skip whitespace
      while ((buf[i] == ' ' || buf[i] == '\t') && (i < nbuf && buf[i] != '\0')) {
        i++;
      }

      if (i >= nbuf || buf[i] == '\0') {
        fprintf(2, "no filename provided\n");
        exit(1);
      }

      int j = 0;
      while (i < nbuf && buf[i] != ' ' && buf[i] != '\t' && buf[i] != '\0' &&
        buf[i] != '|' && buf[i] != ';' && buf[i] != '<' && buf[i] != '>') {

        file_name[j] = buf[i];
        j++;
        i++;
      }
      file_name[j] = '\0';

      if (redirection_left) {
        file_name_l = file_name;
      } else {
        file_name_r = file_name;
      }
    } 
  }

  // If not space at end, null terminate last argument
  if (we) {
    arg_strings[numargs - 1][len] = '\0';
  }

  // "Copy" arguments to arguments array
  for (int i = 0; i < numargs; i++) {
    arguments[i] = arg_strings[i];
  }
  arguments[numargs] = 0;

  /*
    Sequence command. Continue this command in a new process.
    Wait for it to complete and execute the command following ';'.
  */
  if (sequence_cmd) {
    int pid = fork();

    if (pid > 0) {
      wait(0);
      run_command(buf + sequence_cmd, nbuf - sequence_cmd, pcp);
    } else if (pid == 0) {
      run_command(buf, sequence_cmd - 1, pcp);
      exit(0);
    } else {
      fprintf(2, "fork failed\n");
      exit(1);
    }
    sequence_cmd = 0;
  }

  /*
    If this is a redirection command,
    tie the specified files to std in/out.
  */
  if (redirection_left) {
    close(0);
    if (open(file_name_l, O_RDONLY)) {
      fprintf(2, "open %s failed\n", file_name_l);
      exit(1);
    }
  }
  if (redirection_right) {
    close(1);
    if (open(file_name_r, O_WRONLY | O_CREATE | O_TRUNC) < 0) {
      fprintf(2, "open %s failed\n", file_name_r);
      exit(1);
    }
  }

  /* Parsing done. Execute the command. */

  /*
    If this command is a CD command, write the arguments to the pcp pipe
    and exit with '2' to tell the parent process about this.
  */
  if (strcmp(arguments[0], "cd") == 0) {

    if (numargs != 2) {
      fprintf(2, "cd requires exactly one argument\n");
      exit(1);
    }

    write(pcp[1], arguments[1], strlen(arguments[1]) + 1);
    exit(2);
  } else {
    /*
      Pipe command: fork twice. Execute the left hand side directly.
      Call run_command recursion for the right side of the pipe.
    */
    if (pipe_cmd) {
      pipe(p);

      int pid_1 = fork();
      if (pid_1 == 0) {
        // Child process for the left-hand command
        close(1);
        dup(p[1]);

        close(p[0]);
        close(p[1]);

        exec(arguments[0], arguments);
        fprintf(2, "exec %s failed\n", arguments[0]);
        exit(1);
      } else if (pid_1 < 0) {
        fprintf(2, "fork failed\n");
        exit(1);
      }
      
      int pid_2 = fork();
      if (pid_2 == 0) {
        // Child process for the right-hand command
        close(0);
        dup(p[0]);

        close(p[0]);
        close(p[1]);

        run_command(buf + pipe_cmd, nbuf - pipe_cmd, pcp);
        exit(0);
      } else if (pid_2 < 0) {
          fprintf(2, "fork failed\n");
          exit(1);
      }
      close(p[0]);
      close(p[1]);

      wait(0);  
      wait(0);  


    } else {
      int pid = fork();

      if (pid > 0) {
        wait(0);
      } else if (pid == 0) {
        exec(arguments[0], arguments);

        // If exec fails, exit
        fprintf(2, "exec %s failed\n", arguments[0]);
        exit(1);
      } else {
        fprintf(2, "fork failed\n");
        exit(1);
      }
    }
  }
  exit(0);
}

int main(void) {

  static char buf[100];

  int pcp[2];
  pipe(pcp);

  int status;

  /* Read and run input commands. */
  while((status = getcmd(buf, sizeof(buf))) >= 0) {
    if (status == 1) {
      continue;
    }

    int child_status = 0;
    int pid = fork();

    if (pid == 0) {
      run_command(buf, 100, pcp);
    } else if (pid > 0) {
      wait(&child_status);

      /*
        Check if run_command found this is
        a CD command and run it if required.
      */

      if (child_status == 2) {
        char path[100];
        memset(path, 0, 100);

        if (read(pcp[0], path, sizeof(path)) < 0) {
          fprintf(2, "cannot read path\n");
        } else {
          if (chdir(path) < 0) {
            fprintf(2, "cannot cd %s\n", path);
          }
        }
      }
    } else {
      fprintf(2, "fork failed\n");
      exit(1);
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