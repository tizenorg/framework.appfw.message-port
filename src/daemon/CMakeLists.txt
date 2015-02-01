SET(this_target messageportd)

INCLUDE_DIRECTORIES (
	${CMAKE_SOURCE_DIR}/include
	${CMAKE_SOURCE_DIR}/src/inc
	${CMAKE_SOURCE_DIR}/src/daemon/inc
	)

SET(${this_target}_SOURCE_FILES
	messageportd.cpp
	MessagePortIpcServer.cpp
	MessagePortService.cpp
	MessagePortStub.cpp
	)

SET(${this_target}_requires "dlog bundle glib-2.0 aul chromium libsmack pkgmgr pkgmgr-info")

INCLUDE(FindPkgConfig)
pkg_check_modules(${this_target} REQUIRED ${${this_target}_requires})

FOREACH(flag ${${this_target}_CFLAGS})
    SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} ${flag}")
ENDFOREACH(flag)

SET(EXTRA_CFLAGS  "${EXTRA_CFLAGS} -Wall" )

## SET C COMPILER FLAGS
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EXTRA_CFLAGS} -fPIC -Wall -Werror -fvisibility=hidden")
SET(CMAKE_C_FLAGS_DEBUG "-O0 -g")

## SET CPP COMPILER FLAGS
SET(CMAKE_CXX_FLAGS "${OSP_DEBUG_FLAGS} ${CMAKE_CXX_FLAGS} ${EXTRA_CFLAGS} ${OSP_COMPILER_FLAGS} -fvisibility=hidden -fvisibility-inlines-hidden")


## Create Library
ADD_EXECUTABLE(${this_target} ${${this_target}_SOURCE_FILES})

## SET LINKER FLAGS
SET(CMAKE_EXE_LINKER_FLAGS "-Wl,--as-needed -Wl,--rpath=${LIB_INSTALL_DIR}")

TARGET_LINK_LIBRARIES(${this_target} message-port ${${this_target}_LDFLAGS})

SET_TARGET_PROPERTIES(${this_target}
	PROPERTIES
	SKIP_BUILD_RPATH true
	)

INSTALL(TARGETS ${this_target} DESTINATION bin)
