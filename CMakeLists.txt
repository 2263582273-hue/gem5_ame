cmake_minimum_required(VERSION 2.8.13)
project(gem5)

#Bring the headers, such as Student.h into the project
FIND_PACKAGE( Boost 1.6 )
include_directories(${Boost_INCLUDE_DIRS})
include_directories(src)
include_directories(build/RISCV)
include_directories(ext/softfloat)
include_directories(ext)

#Can manually add the sources using the set command as follows:
#set(SOURCES src/mainapp.cpp src/Student.cpp)

#However, the file(GLOB...) allows for wildcard additions:

file(GLOB_RECURSE cpu_sources src/cpu/*.cc src/cpu/*.hh)
file(GLOB_RECURSE pred_sources src/cpu/pred/*.cc src/cpu/pred/*.hh)
file(GLOB_RECURSE ff_sources src/cpu/forwardflow/*.cc src/cpu/forwardflow/*.hh)
file(GLOB arch_sources src/arch/*.cc src/arch/*.hh)
file(GLOB_RECURSE rv_sources src/arch/riscv/*.cc src/arch/riscv/*.hh)
file(GLOB_RECURSE softfloat_sources ext/softfloat/*.c ext/softfloat/*.h)

file(GLOB_RECURSE other_sources
        src/base/*.cc   src/base/*.hh
        src/mem/*.cc    src/mem/*.hh
        src/sim/*.cc    src/sim/*.hh
        )
file(GLOB_RECURSE build_sources
        build/RISCV/enums/*.cc build/RISCV/enums/*.hh
        build/RISCV/config/*.cc build/RISCV/config/*.hh
        build/RISCV/params/*.cc build/RISCV/params/*.hh
        build/RISCV/debug/*.cc build/RISCV/debug/*.hh
        build/RISCV/proto/*.cc build/RISCV/proto/*.hh build/RISCV/proto/*.h
        )

set(sources ${cpu_sources} ${arch_sources} ${rv_sources}
        ${softfloat_sources}
        ${other_sources} ${build_sources}
        ${pred_sources} ${ff_sources})

add_definitions(-DTRACING_ON=true)

add_compile_definitions(__CLION_CODING__)
add_executable(gem5 ${sources})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
