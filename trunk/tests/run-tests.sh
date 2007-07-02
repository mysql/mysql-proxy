#!/bin/sh 

MYSQL_USER=root
MYSQL_PASSWORD=
MYSQL_HOST=127.0.0.1
MYSQL_PORT=3306
MYSQL_DB=test

PROXY_HOST=127.0.0.1
PROXY_PORT=4040

if test x$srcdir = x; then
	srcdir=`dirname $0`
fi

PROXY_PIDFILE=`pwd`/mysql-proxy-test.pid
PROXY_BACKEND_PIDFILE=`pwd`/mysql-proxy-test-backend.pid

## us it to inject strace or valgrind
PROXY_TRACE=
PROXY_BINPATH=$srcdir/../src/mysql-proxy
PROXY_PARAMS=                    ## extra params

## allow local override of the default params
test -x $srcdir/run-tests-conf.sh && . $srcdir/run-tests-conf.sh

## if we have no params run all tests
## otherwise assume we got a test-name like t/select-null.test

run_test() {
	mysqltest \
		--user="$MYSQL_USER" \
		--password="$MYSQL_PASSWORD" \
		--host="$PROXY_HOST" \
		--port="$PROXY_PORT" \
		--test-file="$srcdir/t/$f.test" \
		--result-file="$srcdir/r/$f.result"
}

if test x$PROXY_BACKEND_PORT != x; then
	## start the backend server
	$PROXY_TRACE $PROXY_BINPATH \
		--admin-address="$PROXY_BACKEND_HOST:$PROXY_BACKEND_PORT" \
		--no-proxy \
		--pid-file=$PROXY_BACKEND_PIDFILE
	
	$PROXY_TRACE $PROXY_BINPATH \
		--proxy-backend-addresses="$PROXY_BACKEND_HOST:$PROXY_BACKEND_PORT" \
		--proxy-address="$PROXY_HOST:$PROXY_PORT" \
		--pid-file=$PROXY_PIDFILE \
		$PROXY_PARAMS
else
	## start the proxy only
	$PROXY_TRACE $PROXY_BINPATH \
		--proxy-backend-addresses="$MYSQL_HOST:$MYSQL_PORT" \
		--proxy-address="$PROXY_HOST:$PROXY_PORT" \
		--pid-file=$PROXY_PIDFILE \
		$PROXY_PARAMS
fi

## run the test
if test $# = 0; then
	for i in `ls $srcdir/t/*.test`; do
		f=`basename $i | sed 's/\.test$//'`
		echo -n "[$f] "
		run_test $f
	done
else
	f=`basename $1 | sed 's/\.test$//'`
	run_test $f
fi


## cleanup
if test -e $PROXY_PIDFILE; then
	kill `cat $PROXY_PIDFILE`
	rm $PROXY_PIDFILE
fi

if test -e $PROXY_BACKEND_PIDFILE; then
	kill `cat $PROXY_BACKEND_PIDFILE`
	rm $PROXY_BACKEND_PIDFILE
fi

