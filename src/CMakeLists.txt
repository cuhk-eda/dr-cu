#########
# Setup #
#########

# Specify the minimum version for CMake
# 3.5+ needed for CMAKE_CXX_VISIBILITY_PRESET
cmake_minimum_required(VERSION 3.5.0)

# Message
message(STATUS "ISPD19 DR CUHK")
message(STATUS ${CMAKE_CURRENT_SOURCE_DIR})
message(STATUS ${PROJECT_SOURCE_DIR})

# Find includes in the build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Project's name
project(ispd19dr)

# Set the output folder where your program will be created
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})

set(CMAKE_CXX_STANDARD 14)

set(PATH_RSYN ${CMAKE_CURRENT_SOURCE_DIR}/../rsyn)
set(PATH_ISPD18 ${CMAKE_CURRENT_SOURCE_DIR})

###################
# Warnings/Errors #
###################

set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Werror=return-type")

######################
# Check Dependencies #
######################
find_package(Boost COMPONENTS filesystem program_options REQUIRED)

###############
# Source Code #
###############

find_package(BISON REQUIRED)
set(LEF_SRC_DIR ${PATH_RSYN}/lef/lef)
set(DEF_SRC_DIR ${PATH_RSYN}/def/def)
BISON_TARGET(LEF ${LEF_SRC_DIR}/lef.y ${LEF_SRC_DIR}/lef.tab.c COMPILE_FLAGS "-v -p lefyy -d")
BISON_TARGET(DEF ${DEF_SRC_DIR}/def.y ${DEF_SRC_DIR}/def.tab.c COMPILE_FLAGS "-v -p defyy -d")

file(GLOB_RECURSE SRC_FILES_RSYN
    ${PATH_RSYN}/src/*.cpp
    ${PATH_RSYN}/src/*.cc
    ${PATH_RSYN}/src/*.c
    ${LEF_SRC_DIR}/*.cpp
    ${DEF_SRC_DIR}/*.cpp
    )

file(GLOB_RECURSE SRC_FILES_ISPD18 ${PATH_ISPD18}/*.cpp)

set(SRC_FILES ${SRC_FILES_RSYN} ${SRC_FILES_ISPD18} ${LEF_SRC_DIR}/lef.tab.c ${DEF_SRC_DIR}/def.tab.c)

SET_SOURCE_FILES_PROPERTIES(${LEF_SRC_DIR}/lef.tab.c ${DEF_SRC_DIR}/def.tab.c PROPERTIES LANGUAGE CXX)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)

###########
# Targets #
###########

add_executable(ispd19dr ${SRC_FILES})

#######################
# Include Directories #
#######################

include_directories(${PATH_RSYN}/src)
include_directories(${PATH_RSYN}/src/rsyn/export)
include_directories(${PATH_RSYN}/include)
include_directories(${PATH_RSYN}/lef/include)
include_directories(${PATH_RSYN}/def/include)
include_directories(${PATH_ISPD18})



# Boost
target_include_directories(ispd19dr PUBLIC ${Boost_INCLUDE_DIR})
target_link_libraries(ispd19dr ${Boost_LIBRARIES})
