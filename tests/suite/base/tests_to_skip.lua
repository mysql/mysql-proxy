--[[ $%BEGINLICENSE%$
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

 $%ENDLICENSE%$ --]]
--
-- this file contains a list of tests to skip.
-- Add the tests to skip to the table below
--

tests_to_skip = {
    --  test name          reason
    --  --------------    ---------------------------
        ['dummy']       = 'Too ugly to show',
        ['bug_XYZ']     = 'Nobody cares anymore',
        ['end_session']     = 'Have to figure out the sequence',
        ['bug_30867']   = 'needs backends',
        ['xtab2']   = 'works, but needs a real mysql-server',
        ['select_affected_rows']   = 'needs backends',
        ['client_address_socket'] = 'waiting for bug#38416',
        ['change_user'] = 'works, but needs to run as root and configured with a valid user',
	['bug_45167'] = 'works, but mysqltest cant handle errors in change_user'
}


