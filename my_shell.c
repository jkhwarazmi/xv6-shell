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

  int p[2];
  int pipe_cmd = 0;

  int sequence_cmd = 0;

  int i = 0;
  /* Parse the command character by character. */
  for (; i < nbuf && buf[i] != '\0'; i++) {
    /* Parse the current character and set-up various flags:
       sequence_cmd, redirection, pipe_cmd and similar. */

    /* ##### Place your code here. */

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

      // ##### Place your code here.
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
    sequence_cmd = 0;
    if (fork() != 0) {
      wait(0);
      // ##### Place your code here.
    }
  }

  /*
    If this is a redirection command,
    tie the specified files to std in/out.
  */
  if (redirection_left) {
    // ##### Place your code here.
  }
  if (redirection_right) {
    // ##### Place your code here.
  }

  /* Parsing done. Execute the command. */

  /*
    If this command is a CD command, write the arguments to the pcp pipe
    and exit with '2' to tell the parent process about this.
  */
  if (strcmp(arguments[0], "cd") == 0) {
    // ##### Place your code here.
  } else {
    /*
      Pipe command: fork twice. Execute the left hand side directly.
      Call run_command recursion for the right side of the pipe.
    */
    if (pipe_cmd) {
      // ##### Place your code here.
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

      // ##### Place your code here
    } else {
      fprintf(2, "fork failed\n");
      exit(1);
    }
  }
  exit(0);
}

