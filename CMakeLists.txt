CMAKE_MINIMUM_REQUIRED(VERSION 3.5)
PROJECT(async_read_write VERSION 0.1.0)

include_directories(${PROJECT_SOURCE_DIR}/src)

SET(SOURCES ./src/main.cpp ./src/async_reader.cpp ./src/threadSafeQueue.h ./src/async_reader.h)


ADD_EXECUTABLE(${PROJECT_NAME} ${SOURCES})


# 设置boost
find_package(Boost REQUIRED)
if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
    MESSAGE( STATUS "Boost_INCLUDE_DIRS = ${Boost_INCLUDE_DIRS}.")
    MESSAGE( STATUS "Boost_LIBRARIES = ${Boost_LIBRARIES}.")
    MESSAGE( STATUS "Boost_LIB_VERSION = ${Boost_LIB_VERSION}.")

    IF (WIN32)
        target_link_libraries (${PROJECT_NAME} ${Boost_LIBRARIES})
	    MESSAGE(STATUS "Now is windows")
    ELSEIF (APPLE)
        target_link_libraries (${PROJECT_NAME} boost_system boost_filesystem boost_program_options)
	    MESSAGE(STATUS "Now is Apple systens.")
    ELSEIF (UNIX)
        target_link_libraries (${PROJECT_NAME} boost_system boost_filesystem boost_program_options)
	    MESSAGE(STATUS "Now is UNIX-like OS's.")
    ENDIF ()

endif()

# 允许c++11标准、O3优化、多线程。match选项可避免一些cpu上的问题
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
	SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -march=native -pthread" )
    # target_link_libraries (${PROJECT_NAME} boost_system boost_filesystem boost_program_options)
endif()

#设定可执行二进制文件的目录
SET( EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin) 
