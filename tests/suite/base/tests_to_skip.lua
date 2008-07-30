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
        ['bug_35669']   = 'not fixed yet',
        ['bug_30867']   = 'needs backends',
        ['xtab2']   = 'works, but needs a real mysql-server',
        ['select_affected_rows']   = 'needs backends',
		['client_address_socket'] = 'waiting for bug#38416',

}


