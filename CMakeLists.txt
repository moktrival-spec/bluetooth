cmake_minimum_required(VERSION 3.16)
project(BluetoothGATTServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB2 REQUIRED glib-2.0>=2.56)
pkg_check_modules(GIO REQUIRED gio-2.0>=2.56)
pkg_check_modules(GIO_UNIX REQUIRED gio-unix-2.0>=2.56)

include_directories(${CMAKE_SOURCE_DIR}/include ${GLIB2_INCLUDE_DIRS} ${GIO_INCLUDE_DIRS} ${GIO_UNIX_INCLUDE_DIRS})
link_directories(${GLIB2_LIBRARY_DIRS} ${GIO_LIBRARY_DIRS} ${GIO_UNIX_LIBRARY_DIRS})

add_compile_options(${GLIB2_CFLAGS_OTHER} ${GIO_CFLAGS_OTHER} ${GIO_UNIX_CFLAGS_OTHER})

# 最小版本：创建一个能编译的蓝牙GATT服务器
add_executable(bluetooth_gatt_server_minimal
    src/bluetooth_minimal.cpp
)

target_link_libraries(bluetooth_gatt_server_minimal
    ${GLIB2_LIBRARIES}
    ${GIO_LIBRARIES}
    ${GIO_UNIX_LIBRARIES}
)

add_custom_target(run_minimal
    COMMAND ./bluetooth_gatt_server_minimal
    DEPENDS bluetooth_gatt_server_minimal
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# 测试版本：用于验证D-Bus接口
add_executable(bluetooth_gatt_server_test
    src/bluetooth_test.cpp
)

target_link_libraries(bluetooth_gatt_server_test
    ${GLIB2_LIBRARIES}
    ${GIO_LIBRARIES}
    ${GIO_UNIX_LIBRARIES}
)

add_custom_target(run_test
    COMMAND ./bluetooth_gatt_server_test
    DEPENDS bluetooth_gatt_server_test
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# 修复版本：解决GATT注册问题
add_executable(bluetooth_gatt_server_fixed
    src/bluetooth_fixed.cpp
)

target_link_libraries(bluetooth_gatt_server_fixed
    ${GLIB2_LIBRARIES}
    ${GIO_LIBRARIES}
    ${GIO_UNIX_LIBRARIES}
)

add_custom_target(run_fixed
    COMMAND ./bluetooth_gatt_server_fixed
    DEPENDS bluetooth_gatt_server_fixed
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

install(TARGETS bluetooth_gatt_server_minimal RUNTIME DESTINATION bin)