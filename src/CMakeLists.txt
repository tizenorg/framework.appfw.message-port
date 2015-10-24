SET(this_target message-port)

INCLUDE_DIRECTORIES (
	${CMAKE_SOURCE_DIR}/include
	${CMAKE_SOURCE_DIR}/src/inc
	)

SET(${this_target}_SOURCE_FILES
	message-port.c
	)

SET(${this_target}_requires "dlog bundle glib-2.0 gio-2.0 aul openssl")

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
SET(CMAKE_CXX_FLAGS "${OSP_DEBUG_FLAGS} ${CMAKE_CXX_FLAGS} ${EXTRA_CFLAGS} ${OSP_COMPILER_FLAGS} -fvisibility=hidden")


## Create Library
ADD_LIBRARY (${this_target} SHARED ${${this_target}_SOURCE_FILES})

## SET LINKER FLAGS
SET(CMAKE_SHARED_LINKER_FLAGS "-Wl,--as-needed -Wl,--no-undefined -Wl,--rpath=${LIB_INSTALL_DIR}")

TARGET_LINK_LIBRARIES(${this_target} ${${this_target}_LDFLAGS})

SET_TARGET_PROPERTIES(${this_target}
	PROPERTIES
	VERSION ${FULLVER}
	SOVERSION ${MAJORVER}
	CLEAN_DIRECT_OUTPUT 1
	)

# pkgconfig file
SET(PC_NAME ${this_target})
SET(PACKAGE_DESCRIPTION "Message Port internal library")
SET(VERSION ${FULLVER})
SET(PC_REQUIRED ${${this_target}_requires})
CONFIGURE_FILE(${CMAKE_SOURCE_DIR}/${this_target}.pc.in ${CMAKE_SOURCE_DIR}/${this_target}.pc @ONLY)
INSTALL(FILES ${CMAKE_SOURCE_DIR}/${this_target}.pc DESTINATION ${LIB_INSTALL_DIR}/pkgconfig)

INSTALL(TARGETS ${this_target} DESTINATION ${LIB_INSTALL_DIR})

INSTALL(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION include/appfw FILES_MATCHING PATTERN "*.h")

INSTALL(DIRECTORY ${LIBRARY_OUTPUT_PATH}/ DESTINATION ${LIB_INSTALL_DIR} FILES_MATCHING PATTERN "*.so*")
