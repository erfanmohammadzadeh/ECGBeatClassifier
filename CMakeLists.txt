cmake_minimum_required(VERSION 3.16)

project(BeatClassifier VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Charts)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Charts)

# ---- ONNX Runtime ----
set(ONNX_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/onnxruntime-win-x64-1.31.0")

find_path(ONNX_RUNTIME_INCLUDE_DIRS
    NAMES onnxruntime_cxx_api.h
    HINTS "${ONNX_ROOT}/include"
    REQUIRED)

find_library(ONNX_RUNTIME_LIB
    NAMES onnxruntime
    HINTS "${ONNX_ROOT}/lib"
    REQUIRED)

message(STATUS "ONNX include: ${ONNX_RUNTIME_INCLUDE_DIRS}")
message(STATUS "ONNX lib    : ${ONNX_RUNTIME_LIB}")

set(PROJECT_SOURCES
    main.cpp
    src/csvloader.h      src/csvloader.cpp
    src/preprocessor.h   src/preprocessor.cpp
    src/onnxclassifier.h src/onnxclassifier.cpp
    src/ecgclassifier.h  src/ecgclassifier.cpp
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(BeatClassifier
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        Resource.qrc
    )
else()
    if(ANDROID)
        add_library(BeatClassifier SHARED ${PROJECT_SOURCES})
    else()
        add_executable(BeatClassifier ${PROJECT_SOURCES})
    endif()
endif()

target_include_directories(BeatClassifier PRIVATE ${ONNX_RUNTIME_INCLUDE_DIRS})
target_link_libraries(BeatClassifier PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    ${ONNX_RUNTIME_LIB}
)

target_link_libraries(BeatClassifier PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Charts
    ${ONNX_RUNTIME_LIB}
)

# Copy onnxruntime.dll next to the executable (Windows)
if(WIN32)
    add_custom_command(TARGET BeatClassifier POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ONNX_ROOT}/lib/onnxruntime.dll"
            "$<TARGET_FILE_DIR:BeatClassifier>"
        COMMENT "Copying onnxruntime.dll next to BeatClassifier.exe"
    )
endif()

if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.BeatClassifier)
endif()
set_target_properties(BeatClassifier PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS BeatClassifier
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(BeatClassifier)
endif()