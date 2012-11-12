/* $%BEGINLICENSE%$
 Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License as
 published by the Free Software Foundation; version 2 of the
 License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA

 $%ENDLICENSE%$ */
#ifndef __NETWORK_DEBUG_H__
#define __NETWORK_DEBUG_H__

/**
 * if NETWORK_DEBUG_TRACE_IO is defined, the network layer will log the
 * raw MySQL packets as log-level "debug"
 *
 * #define NETWORK_DEBUG_TRACE_IO 1
 */

/**
 * if NETWORK_DEBUG_TRACE_STATE_CHANGES is defined the state engine for
 * the mysql protocol will log all state changes 
 *
 * #define NETWORK_DEBUG_TRACE_STATE_CHANGES 1
 */

#endif
