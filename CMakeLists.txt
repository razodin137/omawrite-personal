cmake_minimum_required(VERSION 3.21)
project(writecam LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Widgets and PrintSupport are for the system print dialog, which is why the
# app runs a QApplication rather than a QGuiApplication.
find_package(Qt6 REQUIRED COMPONENTS
  Core Gui Concurrent Widgets PrintSupport Qml Quick QuickControls2 QuickDialogs2 DBus Test)

# Everything but main.cpp, shared by the app and the tests, so each source
# compiles once and a new file in src/ needs no line here.
file(GLOB writecam_sources CONFIGURE_DEPENDS src/*.cpp src/*.h)
list(REMOVE_ITEM writecam_sources ${CMAKE_SOURCE_DIR}/src/main.cpp)
add_library(writecam_core STATIC ${writecam_sources})
target_include_directories(writecam_core PUBLIC src)
target_link_libraries(writecam_core PUBLIC
  Qt6::Core Qt6::Gui Qt6::Concurrent Qt6::Widgets Qt6::PrintSupport
  Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::QuickDialogs2 Qt6::DBus)
target_compile_options(writecam_core PUBLIC -Wall -Wextra)

add_executable(writecam src/main.cpp src/resources.qrc)
target_link_libraries(writecam PRIVATE writecam_core)

# The tests read the QML they drive straight from src/.
enable_testing()
add_executable(tst_writecam tests/tst_writecam.cpp)
target_link_libraries(tst_writecam PRIVATE writecam_core Qt6::Test)
add_test(NAME tst_writecam COMMAND tst_writecam)
# An empty platform theme keeps file dialogs in the offscreen window instead
# of opening the real desktop chooser.
set_tests_properties(tst_writecam PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QPA_PLATFORMTHEME=")
