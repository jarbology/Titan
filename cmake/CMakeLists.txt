cmake_minimum_required(VERSION 2.8.8)

add_subdirectory(../deps deps)

project(titan)

set(HOST_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../")

set(TITAN_SRC
	${HOST_ROOT}/src/main.cpp
	${HOST_ROOT}/src/sim.cpp
)

set (TITAN_INCLUDES
	"${MOAI_ROOT}/src"
	"${MOAI_ROOT}/src/moai-config"
	"${MOAI_ROOT}/3rdparty/sdl2-2.0.0/include"
)

include_directories(${TITAN_INCLUDES})
add_executable(titan ${TITAN_SRC})
target_link_libraries(titan 
	FileWatcher
	host-modules
	moai-core
	moai-util
	moai-sim
	SDL2-static
	zlcore
)
