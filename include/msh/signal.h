#ifndef MSH_SIGNAL_H
#define MSH_SIGNAL_H

#include "msh.h"

extern volatile sig_atomic_t msh_got_sigint;
extern volatile sig_atomic_t msh_got_sigtstp;
extern volatile sig_atomic_t msh_got_sigchld;
extern volatile sig_atomic_t msh_got_sighup;

void msh_signal_init(void);
void msh_signal_dispatch(void);
void msh_signal_block(void);
void msh_signal_unblock(void);
void msh_signal_set_fg(pid_t pgid);

#endif