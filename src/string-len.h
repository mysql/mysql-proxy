/* $%BEGINLICENSE%$
 Copyright (C) 2007-2008 MySQL AB, 2008 Sun Microsystems, Inc

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
 

#ifndef _GLIB_EXT_STRING_LEN_H_
#define _GLIB_EXT_STRING_LEN_H_

/**
 * simple macros to get the data and length of a "string"
 *
 * C() is for constant strings like "foo"
 * S() is for GString's 
 */
#define C(x) x, x ? sizeof(x) - 1 : 0
#define S(x) (x) ? (x)->str : NULL, (x) ? (x)->len : 0

#endif
