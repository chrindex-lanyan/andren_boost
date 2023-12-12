#pragma once

#include <signal.h>

typedef void(*sig_callback_t)(int sig , void * param);
typedef void* sig_param_t;

extern void set_signal_callback(sig_callback_t callback);

extern sig_param_t signal_listen(int v, sig_param_t param);

extern int signal_send(int pid, int sig);

extern int self_pid();


