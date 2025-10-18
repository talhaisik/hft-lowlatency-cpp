# CMakeList.txt : CMake project for hft-lowlatency-cpp, include source and define
# project specific logic here.
#

# Add source to this project's executable.
add_executable (hft-lowlatency-cpp "hft-lowlatency-cpp.cpp" "hft-lowlatency-cpp.h")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET hft-lowlatency-cpp PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
