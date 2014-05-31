CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

SET(this_target "message-port")

SET(CMAKE_INSTALL_PREFIX /usr)
SET(PREFIX ${CMAKE_INSTALL_PREFIX})

## OUTPUT PATHS
SET(LIBRARY_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/cmake_build_tmp/output)

INCLUDE_DIRECTORIES (
	/usr/include
	/usr/include/appfw
	/usr/include/dlog
	/usr/include/glib-2.0
	${LIB_INSTALL_DIR}/glib-2.0/include
	/usr/include/chromium
	include
	)

SET (${this_target}_SOURCE_FILES
	src/IpcClient.cpp
	src/message-port.cpp
	src/message-port-messages.cpp
	src/MessagePortProxy.cpp
)

SET(requires "dlog bundle glib-2.0 aul chromium")

INCLUDE(FindPkgConfig)
pkg_check_modules(${this_target} REQUIRED ${requires})
FOREACH(flag ${${this_target}_CFLAGS})
    SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} ${flag}")
ENDFOREACH(flag)

SET(EXTRA_CFLAGS  "${EXTRA_CFLAGS} -Wall" )

## SET C COMPILER FLAGS
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EXTRA_CFLAGS} -fPIC -Wall -Werror -fvisibility=hidden")
SET(CMAKE_C_FLAGS_DEBUG "-O0 -g")

## SET CPP COMPILER FLAGS
SET(CMAKE_CXX_FLAGS "${OSP_DEBUG_FLAGS} ${CMAKE_CXX_FLAGS} ${EXTRA_CFLAGS} ${OSP_COMPILER_FLAGS} -fvisibility=hidden -fvisibility-inlines-hidden")

IF("${ARCH}" STREQUAL "arm")
    ADD_DEFINITIONS("-DTARGET")
ENDIF("${ARCH}" STREQUAL "arm")

ADD_DEFINITIONS("-DPREFIX=\"${CMAKE_INSTALL_PREFIX}\"")
ADD_DEFINITIONS("-DSLP_DEBUG")

## Create Library
ADD_LIBRARY (${this_target} SHARED ${${this_target}_SOURCE_FILES})

## SET LINKER FLAGS
SET(CMAKE_EXE_LINKER_FLAGS "-Wl,--as-needed -Wl,--rpath=${LIB_INSTALL_DIR}")

TARGET_LINK_LIBRARIES(${this_target} "-ldlog" )
TARGET_LINK_LIBRARIES(${this_target} "-lbundle" )
TARGET_LINK_LIBRARIES(${this_target} "-lglib-2.0" )
TARGET_LINK_LIBRARIES(${this_target} "-lchromium" )
TARGET_LINK_LIBRARIES(${this_target} "-laul" )

SET_TARGET_PROPERTIES(${this_target}
	PROPERTIES
	VERSION ${FULLVER}
	SOVERSION ${MAJORVER}
	CLEAN_DIRECT_OUTPUT 1
	)


# pkgconfig file
CONFIGURE_FILE(${this_target}.pc.in ${CMAKE_SOURCE_DIR}/${this_target}.pc @ONLY)
INSTALL(FILES ${CMAKE_SOURCE_DIR}/${this_target}.pc DESTINATION ${LIB_INSTALL_DIR}/pkgconfig)

INSTALL(TARGETS ${this_target} DESTINATION ${LIB_INSTALL_DIR})

INSTALL(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION include/appfw FILES_MATCHING PATTERN "*.h")

INSTALL(DIRECTORY ${LIBRARY_OUTPUT_PATH}/ DESTINATION ${LIB_INSTALL_DIR}
		FILES_MATCHING PATTERN "*.so*" 
		PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ                      
                    GROUP_EXECUTE GROUP_READ
                    WORLD_EXECUTE WORLD_READ)
