
---
-- a lua baed test-runner for the mysql-proxy
--
-- to stay portable it is written in lua
--


-- we require LFS (LuaFileSystem)
require("lfs")

---
-- get the directory-name of a path
--
-- @param filename path to create the directory name from
function dirname(filename)
	local dirname = filename

	attr = assert(lfs.attributes(dirname))

	if attr.mode == "directory" then
		return dirname
	end

	dirname = filename:gsub("/[^/]+$", "")
	
	attr = assert(lfs.attributes(dirname))

	assert(attr.mode == "directory")

	return dirname
end

---
-- get the file-name of a path
--
-- @param filename path to create the directory name from
function basename(filename)
	name = filename:gsub(".*/", "")
	
	return name
end


-- 
-- a set of user variables which can be overwritten from the environment
--

local testdir = dirname(arg[0])

local MYSQL_USER     = os.getenv("MYSQL_USER")     or "root"
local MYSQL_PASSWORD = os.getenv("MYSQL_PASSWORD") or ""
local MYSQL_HOST     = os.getenv("MYSQL_HOST")     or "127.0.0.1"
local MYSQL_PORT     = os.getenv("MYSQL_PORT")     or "3306"
local MYSQL_DB       = os.getenv("MYSQL_DB")       or "test"
local MYSQL_TEST_BIN = os.getenv("MYSQL_TEST_BIN") or "mysqltest"

local PROXY_HOST     = os.getenv("PROXY_HOST")     or "127.0.0.1"
local PROXY_PORT     = os.getenv("PROXY_PORT")     or "14040"
local ADMIN_PORT     = os.getenv("ADMIN_PORT")     or "14041"
local PROXY_TMP_LUASCRIPT = os.getenv("PROXY_TMP_LUASCRIPT") or "/tmp/proxy.tmp.lua"

local srcdir         = os.getenv("srcdir")         or testdir .. "/"
local builddir       = os.getenv("builddir")       or testdir .. "/../"

local PROXY_TRACE    = os.getenv("PROXY_TRACE")    or ""    -- use it to inject strace or valgrind
local PROXY_PARAMS   = os.getenv("PROXY_PARAMS")   or ""    -- extra params
local PROXY_BINPATH  = os.getenv("PROXY_BINPATH")  or builddir .. "/src/mysql-proxy"

local COVERAGE_LCOV  = os.getenv("COVERAGE_LCOV")

--
-- end of user-vars
--

local exitcode=0

local PROXY_PIDFILE  = lfs.currentdir() .. "/mysql-proxy-test.pid"
local PROXY_BACKEND_PIDFILE = lfs.currentdir() .. "/mysql-proxy-test-backend.pid"

---
-- check if the file exists and is readable 
function file_exists(f)
	return lfs.attributes(f)
end

--- 
-- copy a file
--
-- @param dst filename of the destination
-- @param src filename of the source
function file_copy(dst, src)
	local src_fd = assert(io.open(src, "rb"))
	local content = src_fd:read("*a")
	src_fd:close();

	local dst_fd = assert(io.open(dst, "wb"))
	dst_fd:write(content);
	dst_fd:close();
end

---
-- create a empty file
--
-- if the file exists, it will be truncated to 0
--
-- @param dst filename to create and truncate
function file_empty(dst)
	local dst_fd = assert(io.open(dst, "wb"))
	dst_fd:close();
end

---
-- turn a option-table into a string 
--
-- the values are encoded and quoted for the shell
--
-- @param tbl a option table
-- @param sep the seperator, defaults to a space
function options_tostring(tbl, sep)
	-- default value for sep 
	sep = sep or " "
	
	assert(type(tbl) == "table")
	assert(type(sep) == "string")

	local s = ""
	for k, v in pairs(tbl) do
		local enc_value = v:gsub("\\", "\\\\"):gsub("\"", "\\\"")
		s = s .. "--" .. k .. "=\"" .. enc_value .. "\" "
	end

	return s
end

function os_execute(cmdline)
	print("$ " .. cmdline)
	return os.execute(cmdline)
end

--- 
-- run a test
--
-- @param testname name of the test
-- @return exit-code of mysql-test
function run_test(filename, basedir)
	if not basedir then basedir = srcdir end

	local testname = assert(filename:match("t/(.+)\.test"))
	local testfilename = basedir .. "/t/" .. testname .. ".lua"
	if file_exists(testfilename) then
		file_copy(PROXY_TMP_LUASCRIPT, testfilename)
	else
		file_empty(PROXY_TMP_LUASCRIPT)
	end

	local ret = os_execute(MYSQL_TEST_BIN .. " " ..
		options_tostring({
			user     = MYSQL_USER,
			password = MYSQL_PASSWORD,
			database = MYSQL_DB,
			host     = PROXY_HOST,
			port     = PROXY_PORT,
			["test-file"] = basedir .. "/t/" .. testname .. ".test",
			["result-file"] = basedir .. "/r/" .. testname .. ".result"
		})
	)
	
	return ret
end

-- the proxy needs the lua-script to exist
file_empty(PROXY_TMP_LUASCRIPT)

-- if the pid-file is still pointing to a active process, kill it
if file_exists(PROXY_PIDFILE) then
	os.execute("kill -TERM `cat ".. PROXY_PIDFILE .." `")
	os.remove(PROXY_PIDFILE)
end

if COVERAGE_LCOV then
	-- os_execute(COVERAGE_LCOV .. 
	--	" --zerocounters --directory ".. srcdir .. "/../src/" )
end

-- setting the include path
--

-- this is the path containing the global Lua modules
local GLOBAL_LUA_PATH = os.getenv('LUA_LDIR')  or '/usr/share/lua/5.1/?.lua'

-- this is the path containing the Proxy libraries 
local PROXY_LUA_PATH = os.getenv('LUA_PATH')  or '/usr/local/share/?.lua'

-- This is the path with specific libraries for the test suite
local PRIVATE_LUA_PATH = arg[1]  .. '/t/?.lua'  

-- This is the path with additional libraries that the user needs
local LUA_USER_PATH = os.getenv('LUA_USER_PATH')  or '../lib/?.lua'

-- Building the final include path
local INCLUDE_PATH = 
        LUA_USER_PATH     .. ';' ..
        PRIVATE_LUA_PATH  .. ';' ..
        GLOBAL_LUA_PATH   .. ';' .. 
        PROXY_LUA_PATH 

-- start the proxy
assert(os.execute( 'LUA_PATH="' .. INCLUDE_PATH  .. '"  ' ..
    PROXY_TRACE .. " " .. PROXY_BINPATH .. " " ..
	options_tostring({
		["proxy-backend-addresses"] = MYSQL_HOST .. ":" .. MYSQL_PORT,
		["proxy-address"]           = PROXY_HOST .. ":" .. PROXY_PORT,
		["admin-address"]           = PROXY_HOST .. ":" .. ADMIN_PORT,
		["pid-file"]                = PROXY_PIDFILE,
		["proxy-lua-script"]        = PROXY_TMP_LUASCRIPT,
	}) .. " &"
))

--
-- if we have a argument, exectute the named test
-- otherwise execute all tests we can find
if #arg then
	for i, a in ipairs(arg) do
		local stat = lfs.attributes(a)

		if stat.mode == "directory" then
			for file in lfs.dir(a .. "/t/") do
				local testname = file:match("(.+\.test)$")
		
				if testname then
					print("# >> " .. testname .. " started")
		
					local r = run_test("t/" .. testname, a)
					
					print("# << (exitcode = " .. r .. ")" )
		
					if r ~= 0 and exitcode == 0 then
						exitcode = r
					end
				end
			end
		else 
			exitcode = run_test(a)
		end
	end
else
	for file in lfs.dir(srcdir .. "/t/") do
		local testname = file:match("(.+\.test)$")

		if testname then
			print("# >> " .. testname .. " started")

			local r = run_test("t/" .. testname)
			
			print("# << (exitcode = " .. r .. ")" )

			if r ~= 0 and exitcode == 0 then
				exitcode = r
			end
		end
	end
end

-- shut dowm the proxy
--
-- win32 has tasklist and taskkill on the shell
if 0 == os.execute("kill -TERM `cat ".. PROXY_PIDFILE .." `") then
	while 0 == os.execute("kill -0 `cat ".. PROXY_PIDFILE .." ` 2> /dev/null") do
		os.execute("sleep 1")
	end
end
os.remove(PROXY_PIDFILE)

if COVERAGE_LCOV then
	os_execute(COVERAGE_LCOV .. 
		" --quiet " ..
		" --capture --directory ".. srcdir .. "/../src/" ..
		" > " .. srcdir .. "/../tests.coverage.info" )

	os_execute("genhtml " .. 
		"--show-details " ..
		"--output-directory " .. srcdir .. "/../coverage/ " ..
		"--keep-descriptions " ..
		"--frames " ..
		srcdir .. "/../tests.coverage.info")
end


if exitcode == 0 then
	os.exit(0)
else
	print("mysql-test exit-code: " .. exitcode)
	os.exit(-1)
end

