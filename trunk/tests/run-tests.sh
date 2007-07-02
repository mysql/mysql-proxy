#!/bin/sh 

## allow local override of the default params
test -x $srcdir/run-tests-conf.sh && . $srcdir/run-tests-conf.sh

MYSQL_USER=${MYSQL_USER:-root}
MYSQL_PASSWORD=${MYSQL_PASSWORD:-}
MYSQL_HOST=${MYSQL_HOST:-127.0.0.1}
MYSQL_PORT=${MYSQL_PORT:-3306}
MYSQL_DB=${MYSQL_DB:-test}

PROXY_HOST=${PROXY_HOST:-127.0.0.1}
PROXY_PORT=${PROXY_PORT:-4040}
PROXY_TMP_LUASCRIPT=${PROXY_TMP_LUASCRIPT:-/tmp/proxy.tmp.lua}

srcdir=${srcdir:-`dirname $0`}
builddir=${builddir:-`dirname $0`/..}

PROXY_TRACE=${PROXY_TRACE:-}   ## us it to inject strace or valgrind
PROXY_PARAMS=${PROXY_PARAMS:-} ## extra params
PROXY_BINPATH=${PROXY_BINPATH:-$builddir/src/mysql-proxy}

exitcode=0

PROXY_PIDFILE=`pwd`/mysql-proxy-test.pid
PROXY_BACKEND_PIDFILE=`pwd`/mysql-proxy-test-backend.pid

## if we have no params run all tests
## otherwise assume we got a test-name like t/select-null.test

run_test() {
	if test -e $srcdir/t/$f.lua; then
		cp $srcdir/t/$f.lua $PROXY_TMP_LUASCRIPT
	else
		## reset the script
		echo > $PROXY_TMP_LUASCRIPT
	fi

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
	echo > $PROXY_TMP_LUASCRIPT

	$PROXY_TRACE $PROXY_BINPATH \
		--proxy-backend-addresses="$MYSQL_HOST:$MYSQL_PORT" \
		--proxy-address="$PROXY_HOST:$PROXY_PORT" \
		--pid-file=$PROXY_PIDFILE \
		--proxy-lua-script=$PROXY_TMP_LUASCRIPT \
		$PROXY_PARAMS
fi

## run the test
if test $# = 0; then
	for i in `ls $srcdir/t/*.test`; do
		f=`basename $i | sed 's/\.test$//'`
		echo -n "[$f] "
		run_test $f
		exitcode=$?

		if test x$exitcode != x0; then
			break
		fi
	done
else
	f=`basename $1 | sed 's/\.test$//'`
	run_test $f
	exitcode=$?
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

exit $exitcode
