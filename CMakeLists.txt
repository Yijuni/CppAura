cmake_minimum_required(VERSION 3.10)
project(simple_server)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 在文件开头添加 OpenSSL 查找
find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)
find_package(OpenCV REQUIRED)

# 包含头文件
include_directories(
    /usr/local/include # SimpleAmqpClient & ONNX Runtime
    ${OpenCV_INCLUDE_DIRS}
    /usr/include/mysql-cppconn-8
    /usr/include/mysql
    ${OPENSSL_INCLUDE_DIR}
    ${CURL_INCLUDE_DIRS}
)
message(STATUS "OpenCV include dirs: ${OpenCV_INCLUDE_DIRS}")

include_directories(
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/HttpServer/include
    ${PROJECT_SOURCE_DIR}/HttpServer/include/utils
    ${PROJECT_SOURCE_DIR}/HttpServer/include/ssl
    ${PROJECT_SOURCE_DIR}/HttpServer/include/session
    ${PROJECT_SOURCE_DIR}/HttpServer/include/router
    ${PROJECT_SOURCE_DIR}/HttpServer/include/middleware
    ${PROJECT_SOURCE_DIR}/HttpServer/include/middleware/cors
    ${PROJECT_SOURCE_DIR}/HttpServer/include/http
    ${PROJECT_SOURCE_DIR}/AIService/ChatServer/include
    ${PROJECT_SOURCE_DIR}/AIService/ChatServer/include/AIUtil
    ${PROJECT_SOURCE_DIR}/AIService/ChatServer/include/handlers
)

# 查找必要的库
find_library(MYSQLCPPCONN_LIBRARY
    NAMES mysqlcppconn mysqlcppconn8
    PATHS /usr/lib /usr/lib64 /usr/local/lib
)

# 添加所有源文件
file(GLOB_RECURSE HTTP_SERVER_SRC
    "${PROJECT_SOURCE_DIR}/HttpServer/src/*.cpp"
)

file(GLOB_RECURSE GOMOKU_SERVER_SRC
    "${PROJECT_SOURCE_DIR}/AIService/ChatServer/src/*.cpp"
)

# 主程序文件
set(MAIN_SRC "${PROJECT_SOURCE_DIR}/AIService/ChatServer/src/main.cpp")

# 添加可执行文件
add_executable(my_http_server
    ${MAIN_SRC}
    ${HTTP_SERVER_SRC}
    ${GOMOKU_SERVER_SRC}
)
# 链接库
target_link_libraries(my_http_server
    pthread
    muduo_net
    muduo_base
    mysqlcppconn
    mysqlclient
    ssl
    crypto
    ${CURL_LIBRARIES}
    ${OpenCV4_LIBRARIES}
    /usr/lib/x86_64-linux-gnu/libopencv_core.so
    /usr/lib/x86_64-linux-gnu/libopencv_imgproc.so
    /usr/lib/x86_64-linux-gnu/libopencv_highgui.so
    /usr/lib/x86_64-linux-gnu/libopencv_imgcodecs.so
    /usr/lib/x86_64-linux-gnu/libopencv_videoio.so
    /usr/lib/x86_64-linux-gnu/libopencv_dnn.so

    ${PROJECT_SOURCE_DIR}/../onnxruntime-linux-x64-1.20.0/lib/libonnxruntime.so      # 自动查找的 ONNX Runtime 库

    SimpleAmqpClient
    rabbitmq
)

# 如果库不在默认路径，添加链接目录
link_directories(/usr/local/lib)

# 打印调试信息
message(STATUS "Include directories:")
get_property(dirs DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)
foreach(dir ${dirs})
    message(STATUS "  ${dir}")
endforeach()

message(STATUS "Main source file: ${MAIN_SRC}")
message(STATUS "HTTP Server sources: ${HTTP_SERVER_SRC}")
message(STATUS "Gomoku Server sources: ${GOMOKU_SERVER_SRC}")

