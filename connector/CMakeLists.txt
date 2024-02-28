project (connector)

# Tools for using NK parser.
include (platform/nk)

# Set compile flags.
project_header_default ("STANDARD_GNU_11:YES" "STRICT_WARNINGS:NO")

add_executable (Connector "src/connector.c" "src/response-parser.c")
# add_dependencies (Connector traffic_light_idl_files traffic_light_connector_edl_files)
add_dependencies (Connector traffic_light_connector_edl_files)

#set_target_properties (Connector PROPERTIES LINK_FLAGS "-Ttext 0x00800000")

target_include_directories (Connector PRIVATE                            
                            ${CMAKE_SOURCE_DIR}/json-parser)

target_link_libraries (Connector
                       ${${PROJECT_NAME}_LIBS}
                       rump::dhcpcd_client
                       ${vfs_CLIENT_LIB}
                       json_parser
                       m
                       )

# We do not need the default VFS entity which comes from ${vfs_CLIENT_LIB}.
set_target_properties (Connector PROPERTIES ${vfs_ENTITY}_REPLACEMENT "")
set_target_properties (Connector PROPERTIES LINK_FLAGS ${VULN_LDFLAGS})
set_target_properties (Connector PROPERTIES LINK_FLAGS "-no-pie -Ttext 0x100000")

