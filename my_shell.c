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

      // Clear stdin
      while (read(0, &c, 1) > 0 && c != '\n');
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
  int numargs = 0;
  /* Word start/end */
  int ws = 1;
  int we = 0;
  
  int redirection_left = 0;
  int redirection_right = 0;
  char file_name_l[nbuf];
  char file_name_r[nbuf];

  int p[2];
  int pipe_cmd = 0;
  char *pipe_next = 0;

  int sequence_cmd = 0;
  char *sequence_next = 0;

  int i = 0;
  /* Parse the command character by character. */
  for (; i < nbuf && buf[i] != '\0'; i++) {
    /* Parse the current character and set-up various flags:
       sequence_cmd, redirection, pipe_cmd and similar. */

    if (buf[i] == ';') {
      if (i == 0) {
        fprintf(2, "no command\n");
        exit(1);
      }

      sequence_cmd = 1;
      // Store the next command
      sequence_next = &buf[i + 1];
      // End the current command
      buf[i] = '\0';
      break;
    }
    else if (buf[i] == '|') {
      if (i == 0) {
        fprintf(2, "no command\n");
        exit(1);
      }

      pipe_cmd = 1;
      pipe_next = &buf[i + 1];
      buf[i] = '\0';
      break;
    }
    else if (buf[i] == '<') {
      if (i == 0) {
        fprintf(2, "no command\n");
        exit(1);
      }

      // Add support for multiple redirects?
      if (redirection_left) {
        fprintf(2, "cannot have multiple left redirections\n");
        exit(1);
      }
      
      // Keep track of the most recent redirection based on which is higher
      redirection_left = redirection_right + 1;
      buf[i] = '\0';
      i++;
    }
    // > followed by nothing just means create an empty file so valid
    else if (buf[i] == '>') {
      if (redirection_right) {
        fprintf(2, "cannot have multiple right redirections\n");
        exit(1);
      }

      redirection_right = redirection_left + 1;
      buf[i] = '\0';
      i++;
    }

    if (!(redirection_left || redirection_right)) {
      /* No redirection, continue parsing command. */
      
      if (buf[i] == ' ' || buf[i] == '\t') {
        // Set word end
        if (we) {
          buf[i] = '\0';
          we = 0;
          ws = 1;
        }
      } else {
        if (ws) {
          if (numargs >= 10) {
          fprintf(2, "too many arguments\n");
          exit(1);
          }

          arguments[numargs++] = &buf[i];
          ws = 0;
          we = 1;
        }
      }
    } else {
      /* Redirection command. Capture the file names. */

      // Skip whitespace
      while ((buf[i] == ' ' || buf[i] == '\t') && (i < nbuf && buf[i] != '\0')) {
        i++;
      }

      if (i >= nbuf || buf[i] == '\0' || buf[i] == ';' || buf[i] == '|' ||
        buf[i] == '<' || buf[i] == '>') {
        fprintf(2, "no filename provided\n");
        exit(1);
      }

      int j = 0;
      char temp[nbuf];

      while (i < nbuf && buf[i] != ' ' && buf[i] != '\t' && buf[i] != '\0' &&
        buf[i] != '|' && buf[i] != ';' && buf[i] != '<' && buf[i] != '>') {

        temp[j] = buf[i];
        j++;
        i++;
      }
      temp[j] = '\0';

      if (redirection_left > redirection_right) {
        strcpy(file_name_l, temp);
      } else {
        strcpy(file_name_r, temp);
      }

      // Skip any trailing spaces
      while (i < nbuf && (buf[i] == ' ' || buf[i] == '\t')) {
        i++;
      }

      // Go back to check we haven't missed a |, ; or redirect
      i--;
    }
  }
  arguments[numargs] = 0;

  /*
    Sequence command. Continue this command in a new process.
    Wait for it to complete and execute the command following ';'.
  */
  if (sequence_cmd) {
    sequence_cmd = 0;

    // Do not allow this if next command is special character
    while (*sequence_next == ' ' || *sequence_next == '\t') {
      sequence_next++;
    }

    // Remember that ;>filename just creates an empty file so valid
    if (*sequence_next == ';' || *sequence_next == '|' || *sequence_next == '<') {
      fprintf(2, "invalid command following ;\n");
      exit(1);
    }

    int pid = fork();
    if (pid > 0) {
      wait(0);
      run_command(sequence_next, nbuf - (sequence_next - buf), pcp);
      exit(0);
    } else if (pid < 0) {
      fprintf(2, "fork failed\n");
      exit(1);
    }
  }

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

      if (pipe(p) < 0) {
        fprintf(2, "pipe failed\n");
        exit(1);
      }

      int pid_1 = fork();
      if (pid_1 == 0) {
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

      int pid_2 = fork();
      if (pid_2 == 0) {
        close(0);
        dup(p[0]);

        close(p[0]);
        close(p[1]);

        while (*pipe_next == ' ' || *pipe_next == '\t') {
          pipe_next++;
        }
        run_command(pipe_next, nbuf - (pipe_next - buf), pcp);
      }

      close(p[0]);
      close(p[1]);

      wait(0);
      wait(0);

      exit(0);
    } else if (numargs > 0) {
      int pid = fork();

      if (pid == 0) {
        exec(arguments[0], arguments);
        fprintf(2, "exec %s failed\n", arguments[0]);
        exit(1);
      } else if (pid < 0) {
        fprintf(2, "fork failed\n");
        exit(1);
      }

      wait(0);
      exit(0);
    } else {
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

  /* Read and run input commands. */
  int cmd_status;
  while((cmd_status = getcmd(buf, sizeof(buf))) >= 0) {
    // No command or command too long
    if(cmd_status)
      continue;
      
    close(pcp[1]); 
    close(pcp[0]);
    
    if (pipe(pcp) < 0) {
      fprintf(2, "pipe failed\n");
      exit(1);
    }

    if(fork() == 0) {
      close(pcp[0]);
      run_command(buf, sizeof(buf), pcp);
    }

    close(pcp[1]);
    int child_status;
    wait(&child_status);
    
    /*
      Check if run_command found this is
      a CD command and run it if required.
    */
    if(child_status == 2) { 
      char path[100];
      memset(path, 0, sizeof(path));

      if (read(pcp[0], path, sizeof(path)) < 0) {
          fprintf(2, "cannot read path\n");
      } else {
        if (chdir(path) < 0) {
          fprintf(2, "cannot cd %s\n", path);
        }
      }
    }
  }
  exit(0);
}