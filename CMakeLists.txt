# Copyright (C) 2020 Ola Benderius
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

cmake_minimum_required(VERSION 3.2)

project(opendlv-sim-camera-rpi-kiwi)

set(OPENDLV_STANDARD_MESSAGE_SET opendlv-standard-message-set-v0.9.6.odvd)
set(CLUON_COMPLETE cluon-complete-v0.0.127.hpp)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}" ${CMAKE_MODULE_PATH})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s")
set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} \
    -D_XOPEN_SOURCE=700 \
    -D_FORTIFY_SOURCE=2 \
    -O2 \
    -fstack-protector \
    -fomit-frame-pointer \
    -pipe \
    -pedantic -pedantic-errors \
    -Weffc++ \
    -Werror \
    -Wall -Wextra -Wshadow -Wdeprecated \
    -Wdiv-by-zero -Wfloat-equal -Wfloat-conversion -Wsign-compare \
    -Wpointer-arith -Wuninitialized -Wunreachable-code -Wunused \
    -Wunused-function -Wunused-label -Wunused-parameter \
    -Wunused-but-set-parameter -Wunused-but-set-variable -Wunused-value \
    -Wunused-variable -Wunused-result -Wmissing-field-initializers \
    -Wmissing-format-attribute -Wmissing-include-dirs -Wmissing-noreturn")

add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/cluon-msc
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  COMMAND ${CMAKE_COMMAND} -E create_symlink 
  ${CMAKE_CURRENT_SOURCE_DIR}/src/${CLUON_COMPLETE}
  ${CMAKE_BINARY_DIR}/cluon-complete.hpp
  COMMAND ${CMAKE_COMMAND} -E create_symlink 
  ${CMAKE_CURRENT_SOURCE_DIR}/src/${CLUON_COMPLETE}
  ${CMAKE_BINARY_DIR}/cluon-complete.cpp
  COMMAND ${CMAKE_CXX_COMPILER} -o ${CMAKE_BINARY_DIR}/cluon-msc 
  ${CMAKE_BINARY_DIR}/cluon-complete.cpp -pthread -D HAVE_CLUON_MSC
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/${CLUON_COMPLETE})

add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/opendlv-standard-message-set.hpp
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  COMMAND ${CMAKE_BINARY_DIR}/cluon-msc --cpp 
  --out=${CMAKE_BINARY_DIR}/opendlv-standard-message-set.hpp 
  ${CMAKE_CURRENT_SOURCE_DIR}/src/${OPENDLV_STANDARD_MESSAGE_SET}
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/${OPENDLV_STANDARD_MESSAGE_SET} 
  ${CMAKE_BINARY_DIR}/cluon-msc)

include_directories(SYSTEM ${CMAKE_BINARY_DIR})
include_directories(SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
set(LIBRARIES ${LIBRARIES} Threads::Threads)

if(UNIX)
  if(NOT "${CMAKE_SYSTEM_NAME}" STREQUAL "Darwin")
    find_package(LibRT REQUIRED)
    set(LIBRARIES ${LIBRARIES} ${LIBRT_LIBRARIES})
    include_directories(SYSTEM ${LIBRT_INCLUDE_DIR})
  endif()
endif()

set(OpenGL_GL_PREFERENCE "GLVND")
find_package(OpenGL REQUIRED)
include_directories(SYSTEM ${OPENGL_INCLUDE_DIR})
set(LIBRARIES ${LIBRARIES} ${OPENGL_LIBRARIES})

find_package(GLEW REQUIRED)
include_directories(SYSTEM ${GLEW_INCLUDE_DIRS})
set(LIBRARIES ${LIBRARIES} ${GLEW_LIBRARIES})

find_package(X11 REQUIRED)
include_directories(SYSTEM ${X11_INCLUDE_DIRS})
set(LIBRARIES ${LIBRARIES} ${X11_LIBRARIES})

add_executable(${PROJECT_NAME}
  ${CMAKE_CURRENT_SOURCE_DIR}/src/${PROJECT_NAME}.cpp
  ${CMAKE_BINARY_DIR}/opendlv-standard-message-set.hpp)

target_link_libraries(${PROJECT_NAME} ${LIBRARIES})

install(TARGETS ${PROJECT_NAME} DESTINATION bin COMPONENT ${PROJECT_NAME})
