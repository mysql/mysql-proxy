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
#ifndef _CHASSIS_EXPORTS_
#define _CHASSIS_EXPORTS_

#if defined(_WIN32)

#if defined(mysql_chassis_EXPORTS)
#define CHASSIS_API __declspec(dllexport)
#elif defined(mysql_chassis_STATIC)
#define CHASSIS_API 
#else
#define CHASSIS_API extern __declspec(dllimport)
#endif

#else

#define CHASSIS_API		extern

#endif

#endif

