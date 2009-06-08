/**

@page test Testcase List

This is an automatically generated list of all the test cases found.

Currently we have no system for measuring code coverage, but we plan to use a combination of
<a href="http://gcc.gnu.org/onlinedocs/gcc/Gcov.html">gcov</a> and <a href="http://ltp.sourceforge.net/coverage/lcov.php">lcov</a>
in the future.

In order to generate meaningful coverage numbers, especially given that what a system test suite, it is best to generate both
individual coverage reports for each unit test case and combining all coverage reports into a single, global view.

If all the reports are combined into a single report, it is impossible to discern which tests cover which part of the codebase and
thus it becomes hard to identify the areas that need testing. System tests are likely to exercise huge parts of the code and are
not useful when looking at code coverage.

Due to this complexity we haven't put an automated system in place yet.

@see unittests A more detailed description of the individual tests, although only manually tagged tests will show up in that list.

*/