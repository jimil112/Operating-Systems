/*********************************************************************
   Program  : miniShell                   Version    : 1.4
 --------------------------------------------------------------------
   skeleton code for linix/unix/minix command line interpreter
 --------------------------------------------------------------------
   File			: minishell.c
   Compiler/System	: gcc/linux

********************************************************************/

#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

#define NV 20			/* max number of command tokens */
#define NL 100			/* input buffer size */
char            line[NL];	/* command input buffer */

int job_count = 0;

void prompt(void)
{
  // ## REMOVE THIS 'fprintf' STATEMENT BEFORE SUBMISSION
  fprintf(stdout, "\n msh> ");
  fflush(stdout);
}

/* handle finished background processes */
void sigchld_handler(int sig)
{
  int status;
  pid_t pid;
  while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
    job_count++;
    printf("[%d]+ Done                 pid %d\n", job_count, pid);
    fflush(stdout);
  }
}

/* handle Ctrl+C in shell */
void sigint_handler(int sig)
{
  printf("\n msh> ");
  fflush(stdout);
}

/* argk - number of arguments */
/* argv - argument vector from command line */
/* envp - environment pointer */
int main(int argk, char *argv[], char *envp[])
{
   int             frkRtnVal;	    /* value returned by fork sys call */
   char           *v[NV];	        /* array of pointers to command line tokens */
   char           *sep = " \t\n";  /* command line token separators    */
   int             i;		          /* parse index */

   signal(SIGCHLD, sigchld_handler);
   signal(SIGINT, sigint_handler);

    /* prompt for and process one command line at a time  */

  while (1) {			/* do Forever */
    prompt();
    if (fgets(line, NL, stdin) == NULL) {
      if (feof(stdin)) {		/* non-zero on EOF  */
        exit(0);
      }
      perror("fgets");
      continue;
    }
    fflush(stdin);

    // This if() required for gradescope
    if (line[0] == '#' || line[0] == '\n' || line[0] == '\000'){
      continue;			/* to prompt */
    }

    v[0] = strtok(line, sep);
    for (i = 1; i < NV; i++) {
      v[i] = strtok(NULL, sep);
      if (v[i] == NULL){
	      break;
      }
    }
    /* assert i is number of tokens + 1 */

    int background = 0;
    if (i > 0 && v[i-1] && strcmp(v[i-1], "&") == 0) {
      background = 1;
      v[i-1] = NULL;
    }

    if (strcmp(v[0], "cd") == 0) {
      if (v[1] == NULL) {
        if (chdir(getenv("HOME")) == -1) {
          perror("chdir");
        }
      } else {
        if (chdir(v[1]) == -1) {
          perror("chdir");
        }
      }
      continue;
    }

    /* fork a child process to exec the command in v[0] */
    switch (frkRtnVal = fork()) {
      case -1:			/* fork returns error to parent process */
      {
        perror("fork");
	      break;
      }
      case 0:			/* code executed only by child process */
      {
        /* restore default SIGINT behavior for child */
        signal(SIGINT, SIG_DFL);

	      if (execvp(v[0], v) == -1) {
          perror("execvp");
          exit(EXIT_FAILURE);
        }
      }
      default:			/* code executed only by parent process */
      {
        if (background) {
          job_count++;
          printf("[%d] %d\n", job_count, frkRtnVal);
          fflush(stdout);
        } else {
          if (waitpid(frkRtnVal, NULL, 0) == -1) {
            perror("waitpid");
          }
        }
    	  break;
      }
    }				/* switch */
  }				/* while */
}				/* main */
