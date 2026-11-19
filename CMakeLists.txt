cmake_minimum_required(VERSION 4.3)
project(taskkkkk C)

set(CMAKE_C_STANDARD 23)
set(MPI_ROOT "C:/Program Files (x86)/Microsoft SDKs/MPI")
add_executable(taskkkkk main.c)
target_include_directories(taskkkkk PRIVATE
        "${MPI_ROOT}/Include"
)

target_link_directories(taskkkkk PRIVATE
        "${MPI_ROOT}/Lib/x64"
)

target_link_libraries(taskkkkk PRIVATE msmpi)
