//
// tsh - A tiny shell program with job control
//
// <James Alaniz and jalaniz1 here>
//

using namespace std;

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <string>


#include "globals.h"
#include "jobs.h"
#include "helper-routines.h"

//
// Needed global variable definitions - Copied verbatim from shell.c in the provided handout ecf folder - JA
//

static char prompt[] = "tsh> ";
int verbose = 0;            /* if true, print extra output */
char sbuf[MAXLINE];         /* for composing sprintf messages */


/* 
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling actions:
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : fg command
 *     ST -> BG  : bg command
 *     BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 */



// Start my shell - James Alaniz
//
// You need to implement the functions eval, builtin_cmd, do_bgfg,
// waitfg, sigchld_handler, sigstp_handler, sigint_handler
//
// The code below provides the "prototypes" for those functions
// so that earlier code can refer to them. You need to fill in the
// function bodies below.
//

void eval(char *cmdline);
int builtin_cmd(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);//  make fg wait

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

//
// main - The shell's main routine
//
int main(int argc, char **argv)
{
  int emit_prompt = 1; // emit prompt (default)

  //
  // Redirect stderr to stdout (so that driver will get all output
  // on the pipe connected to stdout)
  //
  dup2(1, 2);

  /* Parse the command line */
  char c;
  while ((c = getopt(argc, argv, "hvp")) != EOF) {
  switch (c) {
  case 'h':           // print help message
    usage();
    break;
  case 'v':           // emit additional diagnostic info
    verbose = 1;
    break;
  case 'p':           // don't print a prompt
    emit_prompt = 0;  // handy for automatic testing
    break;
  default:
    usage();
  }
  }

  //
  // Install the signal handlers
  //

  //
  // These are the ones you will need to implement
  //
  Signal(SIGINT,  sigint_handler);   // ctrl-c
  Signal(SIGTSTP, sigtstp_handler);  // ctrl-z
  Signal(SIGCHLD, sigchld_handler);  // Terminated or stopped child

  //
  // This one provides a clean way to kill the shell
  //
  Signal(SIGQUIT, sigquit_handler);

  //
  // Initialize the job list
  //
  initjobs(jobs);

  //
  // Execute the shell's read/eval loop
  //
  for(;;) {
  //
  // Read command line
  //
  if (emit_prompt) {
    printf("%s", prompt);
    fflush(stdout);
  }

  char cmdline[MAXLINE];

  if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin)) {
    app_error("fgets error");
  }
  //
  // End of file? (did user type ctrl-d?)
  //
  if (feof(stdin)) {
    fflush(stdout);
    exit(0);
  }

  //
  // Evaluate command line
  //
  eval(cmdline);
  fflush(stdout);
  fflush(stdout);
  }

  exit(0); //control never reaches here
}
 
/////////////////////////////////////////////////////////////////////////////
//
// eval - Evaluate the command line that the user has just typed in
//
// If the user has requested a built-in command (quit, jobs, bg or fg)
// then execute it immediately. Otherwise, fork a child process and
// run the job in the context of the child. If the job is running in
// the foreground, wait for it to terminate and then return.  Note:
// each child process must have a unique process group ID so that our
// background children don't receive SIGINT (SIGTSTP) from the kernel
// when we type ctrl-c (ctrl-z) at the keyboard.
//
void eval(char *cmdline) // Finished Eval func JA 1:47pm 2/3/2013
{
  /* Parse command line */
  //
  // The 'argv' vector is filled in by the parseline
  // routine below. It provides the arguments needed
  // for the execve() routine, which you'll need to
  // use below to launch a process.
  //
  char *argv[MAXARGS];


  //
  // The 'bg' variable is TRUE if the job should run
  // in background mode or FALSE if it should run in FG
  //
  int bg = parseline(cmdline, argv);

  pid_t pid; // Process ID
  sigset_t mask;

  if (argv[0] == NULL)  
  return;   /* ignore empty lines */
  if (!strcmp(argv[0], "quit"))
    exit(0);// Kill the shell!

  if (!builtin_cmd(argv))
  {
    sigprocmask(SIG_BLOCK, &mask, NULL); /* Block SIGCHLD */
    sigaddset(&mask,SIGCHLD);
    
    if((pid = fork()) == 0) // Create a forked process if pid is zero then we are a child process 
    {
      sigprocmask(SIG_UNBLOCK, &mask, NULL); /* UnBlock SIGCHLD (inherited) */
     setpgid(0,0); // Set group id to zero,zero
    if (bg)
      {
        Signal(SIGINT, SIG_IGN); // Install ignore singal handler
        Signal(SIGTSTP,SIG_IGN); // Install ignore signal handler
      }
      
    if (execve(argv[0],argv, environ )< 0){ // Environ is defined in unistd.h
      printf("%s: Command not found.\n", argv[0]);
      fflush(stdout);
      exit(0); // Kill the forked process!

      }
    }
      /* parent waits for foreground job to terminate or stop */
  int jid = addjob(jobs, pid, (bg == 1 ? BG : FG), cmdline); // Modified addjob to return recently added jid - JA 3:10am 2/3/13
 sigprocmask(SIG_UNBLOCK, &mask, NULL); /* UnBlock SIGCHLD */
  if (!bg) 
      waitfg(pid);
  else
      printf("[%d] (%d) %s",jid, pid, cmdline);
 
  }
  //exit(0);
  return;
}


/////////////////////////////////////////////////////////////////////////////
//
// builtin_cmd - If the user has typed a built-in command then execute
// it immediately. The command name would be in argv[0] and
// is a C string. We've cast this to a C++ string type to simplify
// string comparisons; however, the do_bgfg routine will need
// to use the argv array as well to look for a job number.
//
int builtin_cmd(char **argv) // builtin_cmd finished - JA 1:35pm 2/3/2013
{
  char  *cmd = argv[0];

  if(!strcmp(cmd, "jobs")){ /* jobs command */
    listjobs(jobs);
    return 1;}
  if(!strcmp(cmd, "bg")|| !strcmp(cmd, "fg")){ /* jobs command */
    if (argv[1] == NULL) {
        printf("%s command requires PID or %%jobid argument\n", cmd);
        return 1;
    }
    else
      do_bgfg(argv);
    return 1;}
  if(!strcmp(cmd, "&"))  /* ignore singleton & */
    return 1;
  return 0;   /* not a builtin command */
}

/////////////////////////////////////////////////////////////////////////////
//
// do_bgfg - Execute the builtin bg and fg commands
///////////////////////////////////////////////////////////////////////////
void do_bgfg(char **argv)
{
  struct job_t *jobp=NULL;
    
  /* Parse the required PID or %JID arg */
  if (isdigit(argv[1][0])) {
  pid_t pid = atoi(argv[1]);
  if (!(jobp = getjobpid(jobs, pid))) {
    printf("(%d): No such process\n", pid);
    return;
    }
  }
  else if (argv[1][0] == '%') {
  int jid = atoi(&argv[1][1]);
  if (!(jobp = getjobjid(jobs, jid))) {
    printf("%d: No such job\n", jid);
    return;
    }
   // else
     // printf("Rertrieved job %d with pid %d\n", jid, jobp->pid);
  }      
  else {
    printf("%s: argument must be a PID or %%jobid\n", argv[0]);
    return;
  }

  //
  // You need to complete rest. At this point,
  // the variable 'jobp' is the job pointer
  // for the job ID specified as an argument.
  //
  // Your actions will depend on the specified command
  // so we've converted argv[0] to a string (cmd) for
  // your benefit.
  //
  //string cmd(argv[0]);
  char *cmd = argv[0];

 if (!strcmp(cmd, "fg"))
 {
  pid_t curfg = fgpid(jobs);
  if (curfg == 0) // No current forground process
  {
    kill(jobp->pid,SIGCONT);
    updatejob(jobs,jobp->pid,FG); // Update it, function taken verbatim from shell.c in ECF folder - JA 1:01AM 2/3/2013 
  }
  else
  {
    kill(curfg,SIGTSTP);
    updatejob(jobs,curfg,ST); // Update it, function taken verbatim from shell.c in ECF folder - JA 1:01AM 2/3/2013
    waitfg(curfg);
  
    kill(jobp->pid, SIGCONT); // Allow the designated pid to continue as a foreground process
    updatejob(jobs, jobp->pid,FG);
 }
  }
 if (!strcmp(cmd, "bg"))
 {
  kill(jobp->pid, SIGCONT);
  updatejob(jobs, jobp->pid, BG);
  printf("[%d] (%d) %s ",jobp->jid, jobp->pid, jobp->cmdline);
 }
  return;
}


/////////////////////////////////////////////////////////////////////////////
//
// waitfg - Block until process pid is no longer the foreground process
//
void waitfg(pid_t pid)
{

int status = -1;
// Changed third '0' arg to WUNTRACED (waiting for FG job to stop) - JA 8:47pm 2/2/2013
  if (waitpid(pid,&status,WUNTRACED) < 0)               //wait for pid to 
      unix_error("waitfg: waitpd error");      //no longer be in FG
  

  if (WIFSTOPPED(status))
  {
    status = WSTOPSIG(status);
      int jid = pid2jid(pid); // Map the PID to a JID for output - JA 3:51AM 2/3/13
      printf("Job [%d] (%d) stopped by signal %d\n",jid, pid, status);
    //sprintf(sbuf,"Job %d stopped by a-signal", pid);
    //psignal(WSTOPSIG(status),sbuf);
    updatejob(jobs, pid, ST);
  } // Verbatim - ECF/Shell.c

  else
  {
    if (WIFSIGNALED(status))
    {
      status = WTERMSIG(status);
      int jid = pid2jid(pid); // Map the PID to a JID for output - JA 3:51AM 2/3/13
      printf("Job [%d] (%d) terminated by signal %d\n",jid, pid, status);
      //sprintf(sbuf, "Job %d terminated by b-signal", pid);
      //psignal(WTERMSIG(status), sbuf);
    }
    deletejob(jobs, pid);
    if (verbose)
      printf("waitfg: job %d deleted \n", pid);
  }
  // Implied it returns
}

/////////////////////////////////////////////////////////////////////////////
//
// Signal handlers
//


/////////////////////////////////////////////////////////////////////////////
//
// sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
//  a child job terminates (becomes a zombie), or stops because it
//  received a SIGSTOP or SIGTSTP signal. The handler reaps all
//  available zombie children, but doesn't wait for any other
//  currently running children to terminate.  
//
void sigchld_handler(int sig)
{
  pid_t pid;
  int status;
  if (verbose)
    printf("sigchld_handler: entering\n");

  while((pid = waitpid(-1, &status,WNOHANG|WUNTRACED))>0)
  {
    
  }
  deletejob(jobs, pid);
    if (verbose)
      printf("sigchld_handler: job %d deleted \n", pid);
  if (!((pid == 0) || (pid == -1 && errno == ECHILD)))
    unix_error("sigchld_handler wait error");
  if (verbose)
    printf("sigchld_handler: exiting\n");
  Signal(SIGCHLD, sigchld_handler);  // Re-install
  return;
  // Implied return
}

/////////////////////////////////////////////////////////////////////////////
//
// sigint_handler - The kernel sends a SIGINT to the shell whenever the
//  user types ctrl-c at the keyboard.  Catch it and send it along
//  to the foreground job.  
//
void sigint_handler(int sig)
{
  if (verbose)
    printf("sigint_handler: shell caught SIGINT\n");
   
   pid_t fpid = fgpid(jobs);
   //if (!fpid)
    //unix_error("Fpid is zero");
   
   kill(-fpid, SIGINT);
   updatejob(jobs,fpid,ST); // Stop it
   //Signal(SIGINT,  sigint_handler);   // Re-install ctrl-c
}


/////////////////////////////////////////////////////////////////////////////
//
// sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
//  the user types ctrl-z at the keyboard. Catch it and suspend the
//  foreground job by sending it a SIGTSTP.  
//
void sigtstp_handler(int sig)
{
  if (verbose)
    printf("sigint_handler: shell caught SIGINT\n");
   
   pid_t fpid = fgpid(jobs);
   //if (!fpid)
     // unix_error("Fpid is zero");
  
   kill(-fpid, SIGTSTP);
   //updatejob(jobs,fpid,ST); // Send it to the foreground
  //Signal(SIGTSTP, sigtstp_handler); // Re-install ctrl-z
}








/*********************
 * End signal handlers
 *********************/




