cmake_minimum_required(VERSION 2.8)

macro(QT4_MOC_CPP_FILES)
	foreach(_f ${ARGN})
		get_filename_component(_f_we ${_f} NAME_WE)
		QT4_GENERATE_MOC(${_f} ${CMAKE_CURRENT_BINARY_DIR}/${_f_we}.moc)
		set_source_files_properties(${_f} PROPERTIES OBJECT_DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${_f_we}.moc)
	endforeach()
	include_directories(${CMAKE_CURRENT_BINARY_DIR})
endmacro()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Qt4 REQUIRED QtCore QtDBus)
include(${QT_USE_FILE})

find_package(INotifyTools REQUIRED)
include_directories(${INOTIFYTOOLS_INCLUDE_DIR})

QT4_MOC_CPP_FILES(main.cpp)
add_executable(scs main.cpp)

target_link_libraries(scs ${QT_LIBRARIES} ${INOTIFYTOOLS_LIBRARY})
