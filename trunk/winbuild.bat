@echo "Make sure you have 7za in %CD%\deps\bin or PATH"
@echo "Run this from a shell started with the Visual Studio Build environment set!"
@rem set the right path to the install files
@SET CWD_PATH=%CD%
@SET MYSQL_DIR="C:\Program Files\MySQL\MySQL Server 5.0"
@SET GLIB_DIR=%CWD_PATH%\deps
@SET PATH=%PATH%;%CWD_PATH%\deps\bin;%CWD_PATH%\deps\cmake-2.4.8-win32-x86\bin

@rem MSVC 8 2005 doesn't seem to have devenv.com
@SET VS_CMD="%VS80COMNTOOLS%\..\IDE\VCExpress.exe"

@echo "Unpacking dependencies..."

@7za x -y -odeps deps\cmake-2.4.8-win32-x86.zip
@7za x -y -odeps deps\flex-2.5.4a-1-bin.zip
@7za x -y -odeps deps\tar-1.13-1-bin.zip
@7za x -y -odeps deps\tar-1.13-1-dep.zip
@7za x -y -odeps deps\gzip-1.3.12-1-bin.zip
@7za x -y -odeps deps\glib-2.16.3.zip
@7za x -y -odeps deps\glib-dev-2.16.3.zip

@rem clear the cache if neccesary to let cmake recheck everything
@rem del CMakeCache.txt
 
@cmake -DMYSQL_LIBRARY_DIRS:PATH=%MYSQL_DIR%\lib\debug -DMYSQL_INCLUDE_DIRS:PATH=%MYSQL_DIR%\include -DGLIB_LIBRARY_DIRS:PATH=%GLIB_DIR%\lib -DGLIB_INCLUDE_DIRS:PATH=%GLIB_DIR%\include\glib-2.0;%GLIB_DIR%\lib\glib-2.0\include

%VS_CMD% mysql-proxy.sln /Clean
%VS_CMD% mysql-proxy.sln /Build
%VS_CMD% mysql-proxy.sln /Build Debug /project RUN_TESTS
%VS_CMD% mysql-proxy.sln /Build Debug /project PACKAGE
%VS_CMD% mysql-proxy.sln /Build Debug /project INSTALL
@rem if you use VS8 to build then VS80COMNTOOLS should be set
@rem "%VS80COMNTOOLS%\..\IDE\devenv.com" mysql-proxy.sln /Clean
@rem "%VS80COMNTOOLS%\..\IDE\devenv.com" mysql-proxy.sln /Build
@rem "%VS80COMNTOOLS%\..\IDE\devenv.com" mysql-proxy.sln /Build Debug /project RUN_TESTS
@rem "%VS80COMNTOOLS%\..\IDE\devenv.com" mysql-proxy.sln /Build Debug /project PACKAGE
@rem "%VS80COMNTOOLS%\..\IDE\devenv.com" mysql-proxy.sln /Build Debug /project INSTALL