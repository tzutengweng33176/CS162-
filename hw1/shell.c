#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "tokenizer.h"

/* Convenience macro to silence compiler warnings about unused function parameters. */
#define unused __attribute__((unused))


extern char **environ;

/* Whether the shell is connected to an actual terminal or not. */
bool shell_is_interactive;

/* File descriptor for the shell input */
int shell_terminal;

/* Terminal mode settings for the shell */
struct termios shell_tmodes;

/* Process group id for the shell */
pid_t shell_pgid;

/*getcwd - get the pathname of the current working directory*/
char *getcwd(char *buf, size_t size);

/* chdir, fchdir - change working directory*/
/*

 chdir() changes the current working directory of the calling process
 	to the directory specified in path.
 fchdir() is identical to chdir(); the only difference is that the
 	directory is given as an open file descriptor.
*/
int chdir(const char *path);

/*execute a file*/
int execl(const char *path, const char *arg, ...
                       /* (char  *) NULL */);


int cmd_exit(struct tokens *tokens);
int cmd_help(struct tokens *tokens);
int cmd_cd(struct tokens *tokens);
int cmd_pwd(struct tokens *tokens);



/* Built-in command functions take token array (see parse.h) and return int */
typedef int cmd_fun_t(struct tokens *tokens);

/* Built-in command struct and lookup table */
typedef struct fun_desc {
  cmd_fun_t *fun;
  char *cmd;
  char *doc;
} fun_desc_t;

fun_desc_t cmd_table[] = {
  {cmd_help, "?", "show this help menu"},
  {cmd_exit, "exit", "exit the command shell"},
  {cmd_cd, "cd", "takes one argument, a directory path, and changes the current working directory to that directory"},
  {cmd_pwd, "pwd", " prints the current working directory to standard output."}
};

/* Prints a helpful description for the given command */
int cmd_help(unused struct tokens *tokens) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    printf("%s - %s\n", cmd_table[i].cmd, cmd_table[i].doc);
  return 1;
}

/* Exits this shell */
int cmd_exit(unused struct tokens *tokens) {
  exit(0);
}

/* Changes directory to PATH*/
int cmd_cd(unused struct tokens *tokens) {
   if( tokens_get_length(tokens) != 2 ) {
        fprintf( stderr, "Use: cd <directory>\n" );
        return EXIT_FAILURE;
    }

    if( chdir( tokens_get_token(tokens, 1) ) == 0 ) {
        printf( "Directory changed to %s\n",  tokens_get_token(tokens, 1) );
        return EXIT_SUCCESS;
    } else {
        perror(  tokens_get_token(tokens, 1));
        return EXIT_FAILURE;
    }
}

/* Prints current working directory*/
int cmd_pwd(unused struct tokens *tokens) {
  char cwd[1024];
   if (getcwd(cwd, sizeof(cwd)) != NULL)
       fprintf(stdout, "Current working dir: %s\n", cwd);
   else
       perror("getcwd() error");
   return 0;
  	
}

/* Looks up the built-in command, if it exists. */
int lookup(char cmd[]) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0))
      return i;
  return -1;
}

/* Intialization procedures for this shell */
void init_shell() {
  /* Our shell is connected to standard input. */
  shell_terminal = STDIN_FILENO;

  /* Check if we are running interactively */
  shell_is_interactive = isatty(shell_terminal);

  if (shell_is_interactive) {
    /* If the shell is not currently in the foreground, we must pause the shell until it becomes a
     * foreground process. We use SIGTTIN to pause the shell. When the shell gets moved to the
     * foreground, we'll receive a SIGCONT. */
    while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
      kill(-shell_pgid, SIGTTIN);

    /* Saves the shell's process id */
    shell_pgid = getpid();

    /* Take control of the terminal */
    tcsetpgrp(shell_terminal, shell_pgid);

    /* Save the current termios to a variable, so it can be restored later. */
    tcgetattr(shell_terminal, &shell_tmodes);
  }
}

int main(unused int argc, unused char *argv[]) {
  init_shell();

  static char line[4096];
  int line_num = 0;

  /* Please only print shell prompts when standard input is not a tty */
  if (shell_is_interactive)
    fprintf(stdout, "%d: ", line_num);

  while (fgets(line, 4096, stdin)) {
    /* Split our line into words. */
    struct tokens *tokens = tokenize(line);

    /* Find which built-in function to run. */
    int fundex = lookup(tokens_get_token(tokens, 0));

    if (fundex >= 0) {
      cmd_table[fundex].fun(tokens);
    } else {
      /* REPLACE this to run commands as programs. */
      pid_t pid=fork();
      if(pid==0){
       char *path;
       #define MAX_NAME_SIZE 1000
       char fullName[MAX_NAME_SIZE+1];
       register char *first, *last;
       int nameLength, size, noAccess;

       noAccess = 0;
       int i=0;
       char io[]={'<', '>' };
       while(tokens_get_token(tokens, i)!=NULL){
		if(strcmp(tokens_get_token(tokens, i),&io[0])==0 ){
		int in = open(tokens_get_token(tokens, i+1),O_RDONLY);
                /*printf("%s", "input\n");
		process_ptr->stdin = in;*/
		if(in < 0){
			fprintf(stdin,"no %s file\n",tokens_get_token(tokens, i+1));
			exit(EXIT_FAILURE);
						}
		dup2(in,0);
		close(in);
		/*t[i] = NULL;*/
		break;
		}
	else if(strcmp(tokens_get_token(tokens, i), &io[1])==0){
	int out = open(tokens_get_token(tokens, i+1),O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
        /* printf("%s", "output\n");*/

	dup2(out,1);
	close(out);
	/*t[i] = NULL;*/
	break;
	}
					i++;
		}    


      int ret;
     /* char* name=tokens_get_token(tokens, 0);*/
     /* char *cmd[] = { name, tokens_get_token(tokens, 1), (char *)0 };*/
      char* cmd[4096];
      int c=0;
      while(c< tokens_get_length(tokens)){
         
      if( strcmp(tokens_get_token(tokens, c),&io[0] )==0||strcmp(tokens_get_token(tokens, c),&io[1] )==0){
        break;   /*if we found the string is '<' or '>', break*/
         }
      cmd[c]= tokens_get_token(tokens, c);
           
       
      c++;
}      
      cmd[c+1]= (char*)0;     
 
      char* name= cmd[0];
      if( index(name,'/') != 0) {
	/*
	 * If the name specifies a path, don't search for it on the search path,
	 * just try and execute it.
	 */
          ret=execv(name,cmd );
          return ret;    }
     




       path = getenv("PATH");
    if (path == 0) {
	path = "/sprite/cmds";
    }
    nameLength = strlen(name);
    for (first = path; ; first = last+1) {

	/*
	 * Generate the next file name to try.
	 */

	for (last = first; (*last != 0) && (*last != ':'); last++) {
	    /* Empty loop body. */
	}
	size = last-first;
	if ((size + nameLength + 2) >= MAX_NAME_SIZE) {
	    continue;
	}
	(void) strncpy(fullName, first, size);
	if (last[-1] != '/') {
	    fullName[size] = '/';
	    size++;
	}
	(void) strcpy(fullName + size, name);

	execv(fullName,cmd);
	if (errno == EACCES) {
	    noAccess = 1;
	} else if (errno != ENOENT) {
	    break;
	}
	if (*last == 0) {
	    /*
	     * Hit the end of the path. We're done.
	     * If there existed a file by the right name along the search path,
	     * but its permissions were wrong, return FS_NO_ACCESS. Else return
	     * whatever we just got back.
	     */
	    if (noAccess) {
		errno = EACCES;
	    }
	    break;
	}
    }
    return -1; 
}
    /* fprintf(stdout, "This shell doesn't know how to run programs.\n");*/
    }

    if (shell_is_interactive)
      /* Please only print shell prompts when standard input is not a tty */
      fprintf(stdout, "%d: ", ++line_num);

    /* Clean up memory */
    tokens_destroy(tokens);
  }

  return 0;
}
