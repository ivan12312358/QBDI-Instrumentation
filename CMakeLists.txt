cmake_minimum_required(VERSION 3.16)
project(instrumentation)

add_executable(${PROJECT_NAME} 
  instrumentation.cpp
  test.cpp
)

target_link_libraries(${PROJECT_NAME} QBDI)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 11
CXX_STANDARD_REQUIRED ON)

if(QBDI_TOOLS_QBDIPRELOAD)
  add_library(tracer_preload_cpp SHARED tracer_preload.cpp)
  target_link_libraries(tracer_preload_cpp QBDIPreload QBDI_static)
  set_target_properties(tracer_preload_cpp PROPERTIES CXX_STANDARD 11
                                                      CXX_STANDARD_REQUIRED ON)
endif()
