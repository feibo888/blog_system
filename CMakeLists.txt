cmake_minimum_required(VERSION 3.16.3)
project(blog_system)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")

# 检查Boost版本并添加json组件
set(Boost_COMPONENTS regex)
find_package(Boost COMPONENTS ${Boost_COMPONENTS})
if(Boost_FOUND AND Boost_VERSION VERSION_GREATER_EQUAL 1.75.0)
    list(APPEND Boost_COMPONENTS json)
    find_package(Boost REQUIRED COMPONENTS ${Boost_COMPONENTS})
    add_compile_definitions(HAS_BOOST_JSON)
endif()

include_directories(${Boost_INCLUDE_DIRS})
include_directories(/usr/include/mysql)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/buffer BUFFER_SRC)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/http HTTP_SRC)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/log LOG_SRC)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/pool POLL_SRC)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/server SERVER_SRC)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/timer TIMER_SRC)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/blog BLOG_SRC)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/bin)

add_executable(${PROJECT_NAME}
                main.cpp
                ${BUFFER_SRC}
                ${HTTP_SRC}
                ${LOG_SRC}
                ${POLL_SRC}
                ${SERVER_SRC}
                ${TIMER_SRC}
                ${BLOG_SRC})

target_link_libraries(${PROJECT_NAME} mysqlclient pthread ${Boost_LIBRARIES})