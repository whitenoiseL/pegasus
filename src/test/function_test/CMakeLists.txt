set(MY_PROJ_NAME "pegasus_function_test")
project(${MY_PROJ_NAME} C CXX)

# Source files under CURRENT project directory will be automatically included.
# You can manually set MY_PROJ_SRC to include source files under other directories.
set(MY_PROJ_SRC "")

# Search mode for source files under CURRENT project directory?
# "GLOB_RECURSE" for recursive search
# "GLOB" for non-recursive search
set(MY_SRC_SEARCH_MODE "GLOB")

set(MY_PROJ_INC_PATH "../../include")

set(MY_BOOST_PACKAGES "system")
set(MY_PROJ_LIBS
    dsn.replication.ddlclient
    dsn.replication.clientlib
    pegasus_client_static
    fmt
    )

if (UNIX)
    set(MY_PROJ_LIBS ${MY_PROJ_LIBS} gtest pthread)
else()
    set(MY_PROJ_LIBS ${MY_PROJ_LIBS} gtest)
endif()

set(MY_BOOST_PACKAGES system filesystem)

set(MY_PROJ_LIB_PATH "../..")

set(MY_BINPLACES "config.ini" "run.sh")

dsn_add_executable()
