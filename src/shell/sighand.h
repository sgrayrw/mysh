#ifndef SIGHAND_H
#define SIGHAND_H

#define SIG_MIN 1

// sighandlers
void initialize_handlers(); // register for signal handlers using sigaction(), initialize sigset for SIGCHLD to protect critical section on job list
void sigint_handler(int); // for ctrl-c
void sigchld_handler(int, siginfo_t *, void *); // for child status changes

#endif
