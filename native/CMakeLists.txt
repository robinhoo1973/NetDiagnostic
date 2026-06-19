cmake_minimum_required(VERSION 3.14)
project(nd_plugin LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

set(COMMON_SRC common/nd_factory.cpp common/nd_diagnostic_base.cpp common/nd_plugin.cpp)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT ANDROID)
    set(PLATFORM_SRC linux/nd_network_adapters.cpp linux/nd_nic_advanced.cpp linux/nd_wifi.cpp linux/nd_wired.cpp linux/nd_g2.cpp linux/nd_g3_g4.cpp)
elseif(APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(PLATFORM_SRC macos/nd_diagnostics.mm)
elseif(WIN32)
    set(PLATFORM_SRC windows/nd_diagnostics.cpp)
elseif(ANDROID)
    set(PLATFORM_SRC android/nd_diagnostics.cpp)
endif()

# Build as STATIC library — linked directly into the Qt executable
add_library(nd_plugin STATIC ${COMMON_SRC} ${PLATFORM_SRC})
target_include_directories(nd_plugin PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Platform-specific libraries
if(APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    target_link_libraries(nd_plugin PUBLIC
        "-framework CoreWLAN"
        "-framework IOKit"
        "-framework SystemConfiguration"
        "-framework Foundation"
    )
elseif(WIN32)
    target_link_libraries(nd_plugin PUBLIC
        ws2_32 iphlpapi wlanapi
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Wireless Extensions for WiFi diagnostics
    target_link_libraries(nd_plugin PUBLIC
        pthread
    )
endif()
