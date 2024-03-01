cmake_minimum_required (VERSION 3.12)

project (traffic_light)

# Initialize CMake library for KasperskyOS SDK.
include (platform)
initialize_platform (FORCE_STATIC)

# Add functions for installing artefacts.
include (platform/install)

# Add project tests.
include (platform/tests)
add_project_tests_main_target ()


# Add Doxygen documentation
# include (platform/doxygen)
# add_project_documentation_main_target ()

# Platform definitions.
include (platform/options)

# Tools for using NK parser.
include (platform/nk)

# Add functions for installing artefacts.
include (platform/install)

set (CMAKE_INCLUDE_CURRENT_DIR ON)

# Add package for working with the virtual file system.
find_package (vfs REQUIRED)
include_directories (${vfs_INCLUDE})

# Add a package with the dhcpcd program implementation.
find_package (rump REQUIRED COMPONENTS DHCPCD_ENTITY)
include_directories (${rump_INCLUDE})

# Add package with VFS program implementations.
find_package (precompiled_vfs REQUIRED)
include_directories (${precompiled_vfs_INCLUDE})

# Choose configuration files and the network configuration mode.
# If none of the options are selected, the ramfs device and the dynamic
# network configuration with fallback to static mode are used.
option (DHCPCD_FALLBACK "Configuration by /etc/dhcpcd.conf"           OFF)
option (DHCPCD_DYNAMIC  "Configuration by /etc/dhcpcd_dynamic.config" OFF)
option (DHCPCD_STATIC   "Configuration by /etc/dhcpcd_static.config"  OFF)

# Attach the directory with the generated config header files.
include_directories (include
                     json_parser
                     ${CMAKE_CURRENT_BINARY_DIR})

set (RESOURCES ${CMAKE_SOURCE_DIR}/resources)

# Recommended flags against vulnerabilities.
# Recommended compiler flags against vulnerabilities.
set(VULN_LDFLAGS "\
    -Wl,-z,noexecstack \
    -Wl,-z,separate-code \
    -Wl,-z,now")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} \
    -Wall -Wextra -Wconversion \
    -fPIE -pie -g \
    -fstack-protector-strong -fstack-clash-protection \
    -mbranch-protection=standard \
    -Wsign-conversion -Wformat=2 -Wformat-security -Werror=format-security \
    -fsanitize=undefined -fsanitize-undefined-trap-on-error")

include_directories ( include json-parser )

nk_build_idl_files (traffic_light_idl_files NK_MODULE "traffic_light" IDL "${RESOURCES}/idl/ILightsMode.idl")
nk_build_cdl_files (traffic_light_cdl_files IDL_TARGET traffic_light_idl_files NK_MODULE "traffic_light" CDL "${RESOURCES}/cdl/LightsMode.cdl")

nk_build_idl_files (traffic_light_idl_files NK_MODULE "traffic_light" IDL "${RESOURCES}/idl/ICrossMode.idl")
nk_build_cdl_files (traffic_light_cdl_files IDL_TARGET traffic_light_idl_files NK_MODULE "traffic_light" CDL "${RESOURCES}/cdl/CrossMode.cdl")

nk_build_idl_files (traffic_light_idl_files NK_MODULE "traffic_light" IDL "${RESOURCES}/idl/IEventLog.idl")
nk_build_cdl_files (traffic_light_cdl_files IDL_TARGET traffic_light_idl_files NK_MODULE "traffic_light" CDL "${RESOURCES}/cdl/EventLog.cdl")

nk_build_idl_files (traffic_light_idl_files NK_MODULE "traffic_light" IDL "${RESOURCES}/idl/ITrafficMode.idl")
nk_build_cdl_files (traffic_light_cdl_files IDL_TARGET traffic_light_idl_files NK_MODULE "traffic_light" CDL "${RESOURCES}/cdl/TrafficMode.cdl")

nk_build_edl_files (traffic_light_cross_checker_edl_files CDL_TARGET traffic_light_cdl_files NK_MODULE "traffic_light" EDL "${RESOURCES}/edl/CrossChecker.edl")
nk_build_edl_files (traffic_light_diagnostics_edl_files CDL_TARGET traffic_light_cdl_files NK_MODULE "traffic_light" EDL "${RESOURCES}/edl/Diagnostics.edl")
nk_build_edl_files (traffic_light_lights_gpio_edl_files CDL_TARGET traffic_light_cdl_files NK_MODULE "traffic_light" EDL "${RESOURCES}/edl/LightsGPIO.edl")
nk_build_edl_files (traffic_light_control_system_edl_files CDL_TARGET traffic_light_cdl_files NK_MODULE "traffic_light" EDL "${RESOURCES}/edl/ControlSystem.edl")
nk_build_edl_files (traffic_light_connector_edl_files CDL_TARGET traffic_light_cdl_files NK_MODULE "traffic_light" EDL "${RESOURCES}/edl/Connector.edl")

add_subdirectory (control_system)
add_subdirectory (cross_checker)
add_subdirectory (diagnostics)
add_subdirectory (lights_gpio)
add_subdirectory (connector)
add_subdirectory (json_parser)
add_subdirectory (einit)
