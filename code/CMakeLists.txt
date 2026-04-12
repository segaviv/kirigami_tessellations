cmake_minimum_required(VERSION 3.5)
include(FetchContent)
 
project(kiri_patterns)
 
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
 

FetchContent_Declare(
    libigl
    GIT_REPOSITORY https://github.com/libigl/libigl.git
    GIT_TAG v2.5.0
)
# Optiz
FetchContent_Declare(
  Optiz
  GIT_REPOSITORY https://github.com/segaviv/optiz.git
  GIT_TAG master
)
 
FetchContent_MakeAvailable(Optiz libigl)
# Eigen3
find_package(Eigen3 QUIET)
if(NOT TARGET Eigen3::Eigen)
  # Eigen not found, fetch it from gitlab.
  FetchContent_Declare(
    Eigen3
    GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
    GIT_TAG 3.4.0
    SOURCE_SUBDIR cmake # Hack to avoid Eigen's CMakeLists.txt
  )
  FetchContent_MakeAvailable(Eigen3)
  add_library(Eigen3::Eigen INTERFACE IMPORTED)
  set_target_properties(Eigen3::Eigen PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                                                 ${eigen3_SOURCE_DIR})
endif()
 
file(GLOB CORE_SRC cpp/*.cpp cpp/parameterization/*.cpp cpp/opt/*.cpp)
list(REMOVE_ITEM CORE_SRC "${CMAKE_CURRENT_SOURCE_DIR}/cpp/bind.cpp")
list(REMOVE_ITEM CORE_SRC "${CMAKE_CURRENT_SOURCE_DIR}/cpp/main_batch.cpp")

add_executable(${PROJECT_NAME} ${CORE_SRC} cpp/bind.cpp)
target_link_libraries(${PROJECT_NAME} PUBLIC Eigen3::Eigen Optiz igl::core)
target_link_options(${PROJECT_NAME} PRIVATE -sMODULARIZE -sEXPORT_NAME="createModule" --bind  -sWASM=1 -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=5MB  -sNO_EXIT_RUNTIME=0 -sASSERTIONS=2)

add_executable(kirigami_batch ${CORE_SRC} cpp/main_batch.cpp)
target_link_libraries(kirigami_batch PUBLIC Eigen3::Eigen Optiz igl::core)

add_custom_command(TARGET ${PROJECT_NAME}
POST_BUILD
COMMAND echo 'export default createModule;' >> ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.js)
add_custom_command(TARGET ${PROJECT_NAME}
POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.js ${CMAKE_CURRENT_SOURCE_DIR}/)
add_custom_command(TARGET ${PROJECT_NAME}
POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.wasm ${CMAKE_CURRENT_SOURCE_DIR}/)