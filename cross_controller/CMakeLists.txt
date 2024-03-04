project (CrossController)

# Tools for using NK parser.
include (platform/nk)

# Set compile flags.
project_header_default ("STANDARD_GNU_11:YES" "STRICT_WARNINGS:NO")

add_executable (CrossController "src/cross_controller.c")
add_dependencies (CrossController traffic_light_idl_files traffic_light_cross_controller_edl_files)

target_link_libraries (CrossController ${vfs_CLIENT_LIB})

# We do not need default VFS entity here, which comes from ${vfs_CLIENT_LIB}
set_target_properties (CrossController PROPERTIES ${vfs_ENTITY}_REPLACEMENT "")
set_target_properties (CrossController PROPERTIES LINK_FLAGS ${VULN_LDFLAGS})

