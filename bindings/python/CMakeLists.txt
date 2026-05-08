cmake_minimum_required(VERSION 3.25)

find_package(SWIG 4.0 REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter Development)
include(UseSWIG)

set_property(SOURCE dcpdoctor.i PROPERTY CPLUSPLUS ON)
set_property(SOURCE dcpdoctor.i PROPERTY USE_TARGET_INCLUDE_DIRECTORIES ON)

swig_add_library(pydcpdoctor
  TYPE SHARED
  LANGUAGE python
  SOURCES dcpdoctor.i
)

target_include_directories(pydcpdoctor PRIVATE
  ${PROJECT_SOURCE_DIR}/include
  ${Python3_INCLUDE_DIRS}
)

target_link_libraries(pydcpdoctor PRIVATE
  dcpdoctor_core
  Python3::Python
)

set_target_properties(pydcpdoctor PROPERTIES
  OUTPUT_NAME "_dcpdoctor"
  PREFIX ""
)

if(Python3_SOABI)
  set_target_properties(pydcpdoctor PROPERTIES SUFFIX ".${Python3_SOABI}.so")
endif()

install(TARGETS pydcpdoctor DESTINATION ${Python3_SITEARCH})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dcpdoctor.py DESTINATION ${Python3_SITEARCH})
