cmake_minimum_required(VERSION 2.8.3)
project(libgpc)

set(catkin_RUN_PACKAGES
)

set(catkin_BUILD_PACKAGES 
	${catkin_RUN_PACKAGES}
	cmake_modules
)

## Find catkin macros and libraries
## if COMPONENTS list like find_package(catkin REQUIRED COMPONENTS xyz)
## is used, also find other catkin packages
## Creates a bunch of environment variables that may be used later in the script
## e.g. catkin_INCLUDE_DIRS
find_package(catkin REQUIRED  COMPONENTS
	${catkin_BUILD_PACKAGES}
)

catkin_package(
	INCLUDE_DIRS
		common/include
	LIBRARIES
		gpc
	CATKIN_DEPENDS
		${catkin_RUN_PACKAGES}
	DEPENDS
)

include_directories(
	common/include
)

add_library(gpc common/src/gpc.c)
add_dependencies(gpc ${catkin_EXPORTED_TARGETS})
target_link_libraries(gpc ${catkin_LIBRARIES})

#############
## Install ##
#############
## Mark executables and/or libraries for installation
install(TARGETS gpc
	ARCHIVE DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
	LIBRARY DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
	RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
)

install(DIRECTORY common/include/${PROJECT_NAME}/
	DESTINATION ${CATKIN_PACKAGE_INCLUDE_DESTINATION}
)