/* $%BEGINLICENSE%$
 Copyright (C) 2008 MySQL AB, 2008 Sun Microsystems, Inc

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 $%ENDLICENSE%$ */
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

#define	MYSQLPROXY_STATE_CHANGE_ENABLED() FALSE
#define	MYSQLPROXY_STATE_CHANGE(arg0, arg1, arg2)

#endif
