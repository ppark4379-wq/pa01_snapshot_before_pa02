cmake_minimum_required(VERSION 3.0.2)
project(cartographer_parallel)

option(BUILD_CUDA_TASK "Build optional CUDA assignment library" ON)

find_package(catkin REQUIRED COMPONENTS
  roscpp
  std_msgs
  sensor_msgs
  geometry_msgs
  nav_msgs
  visualization_msgs
)

find_package(OpenMP)

if(OpenMP_CXX_FOUND)
  message(STATUS "OpenMP found")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
endif()

catkin_package(
  INCLUDE_DIRS include
  LIBRARIES assignment_cpu_lib fast_matcher_lib
  CATKIN_DEPENDS roscpp std_msgs sensor_msgs geometry_msgs nav_msgs visualization_msgs
)

include_directories(
  include
  ${catkin_INCLUDE_DIRS}
)

add_library(assignment_cpu_lib
  src/score_all.cpp
)

target_link_libraries(assignment_cpu_lib ${catkin_LIBRARIES})

if(BUILD_CUDA_TASK)

  enable_language(CUDA)

  add_library(assignment_cuda_lib
    src/assignment_cuda.cu
  )

  target_include_directories(assignment_cuda_lib PRIVATE include)

  set_target_properties(assignment_cuda_lib PROPERTIES
    CUDA_SEPARABLE_COMPILATION ON
  )

endif()

add_library(fast_matcher_lib
  src/fast_matcher.cpp
)

if(BUILD_CUDA_TASK)
  target_link_libraries(
    fast_matcher_lib
    assignment_cpu_lib
    assignment_cuda_lib
    ${catkin_LIBRARIES}
  )
else()
  target_link_libraries(
    fast_matcher_lib
    assignment_cpu_lib
    ${catkin_LIBRARIES}
  )
endif()

add_executable(fast_correlative_node
  src/fast_correlative_node.cpp
)

target_link_libraries(
  fast_correlative_node
  fast_matcher_lib
  ${catkin_LIBRARIES}
)

install(TARGETS
  assignment_cpu_lib
  fast_matcher_lib
  fast_correlative_node
  ARCHIVE DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  LIBRARY DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
)

if(BUILD_CUDA_TASK)

  install(TARGETS assignment_cuda_lib
    ARCHIVE DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
    LIBRARY DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  )

endif()

install(DIRECTORY launch maps bags
  DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}
)
