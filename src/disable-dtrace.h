/* short out DTrace macros if we don't have or want DTrace support */
#ifndef ENABLE_DTRACE

#define DTRACE_PROBE(provider, name)
#define DTRACE_PROBE1(provider, name, arg0)
#define DTRACE_PROBE2(provider, name, arg0, arg1)
#define DTRACE_PROBE3(provider, name, arg0, arg1, arg2)
#define DTRACE_PROBE4(provider, name, arg0, arg1, arg2, arg3)
#define DTRACE_PROBE5(provider, name, arg0, arg1, arg2, arg3, arg4)
#define DTRACE_PROBE6(provider, name, arg0, arg1, arg2, arg3, arg4, arg5)
#define DTRACE_PROBE7(provider, name, arg0, arg1, arg2, arg3, arg4, arg5, arg6)
#define DTRACE_PROBE8(provider, name, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7)
#define DTRACE_PROBE9(provider, name, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8)
#define DTRACE_PROBE10(provider, name, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9)

/* when adding new DTrace USDT probes, also add the stubs below */

#define	MYSQLPROXY_STATE_CHANGE_ENABLED()
#define	MYSQLPROXY_STATE_CHANGE(arg0, arg1, arg2)

#endif
