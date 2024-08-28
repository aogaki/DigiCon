# ----------------------------------------------------------------------------
# Setup the project
cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
project(digi-con)
set(LIB_NAME DigiCon)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)

# ---Locate the ROOT package and defines a number of variables (e.g. ROOT_INCLUDE_DIRS)
list(APPEND CMAKE_PREFIX_PATH $ENV{ROOTSYS})

# set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "$ENV{ROOTSYS}/ect/cmake")
find_package(ROOT REQUIRED COMPONENTS RIO Net RHTTP)
include(${ROOT_USE_FILE})

set(CMAKE_CXX_FLAGS_DEBUG_INIT "-Wall")
set(CMAKE_CXX_FLAGS_RELEASE_INIT "-Wall")

# Huuuuuge warning options!
add_compile_options(-O2 -fopenmp -pthread -Wall -Wextra -Wpedantic -Wshadow)

# ----------------------------------------------------------------------------
# Locate sources and headers for this project
# NB: headers are included so they will show up in IDEs
#
include_directories(${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${ROOT_INCLUDE_DIR})
link_directories(${ROOT_LIBRARY_DIR})
file(GLOB headers ${PROJECT_SOURCE_DIR}/include/*.hpp /usr/include/CAEN* ${PROJECT_SOURCE_DIR}/include/*.h)
file(GLOB sources ${PROJECT_SOURCE_DIR}/src/*.cpp)

# Copy config files *.conf to build directory
# file(GLOB conf_files ${PROJECT_SOURCE_DIR}/*.conf)
# file(COPY ${conf_files} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# ----------------------------------------------------------------------------
# Add the executable, and link it to the Geant4 libraries
#
# add_library(TDigiTES STATIC ${sources} ${headers})
# add_library(TDigiTES SHARED ${sources} ${headers})
# target_link_libraries(TDigiTES CAENDigitizer CAENComm CAENVME)
# add_executable(digi main.cpp ${headers})
# target_link_libraries(digi ${ROOT_LIBRARIES} CAENDigitizer CAENComm CAENVME RHTTP TDigiTES)
add_library(${LIB_NAME} SHARED ${sources} ${headers})
target_link_libraries(${LIB_NAME} ${ROOT_LIBRARIES} RHTTP CAEN_FELib gomp)
add_executable(${PROJECT_NAME} main.cpp ${headers})
target_link_libraries(${PROJECT_NAME} ${LIB_NAME})
