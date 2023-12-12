#include "sys_local_ipc.h"
#include <signal.h>
#include <unistd.h>

static sig_callback_t sig_callback = 0;
static sig_param_t sig_param = 0;

void set_signal_callback(sig_callback_t callback)
{
    sig_callback = callback;
}

void _sig_callback_(int sig)
{
    if (sig_callback)
    {
        sig_callback(sig ,sig_param);
    }
}

sig_param_t signal_listen(int v, sig_param_t param)
{
    sig_param_t old = sig_param;
    sig_param = param;
    signal(v,_sig_callback_);
    return old;
}

int signal_send(int pid, int sig)
{
    return kill(pid, sig);
}

int self_pid()
{
    return getpid();
}