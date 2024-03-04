project (CrossController)

# Tools for using NK parser.
include (platform/nk)

# Set compile flags.
project_header_default ("STANDARD_GNU_11:YES" "STRICT_WARNINGS:NO")

add_executable (CrossController "src/cross_controller.c")
add_dependencies (CrossController traffic_light_idl_files traffic_light_cross_controller_edl_files)