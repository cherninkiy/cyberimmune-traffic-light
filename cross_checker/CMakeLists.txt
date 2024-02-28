project (CrossChecker)

# Tools for using NK parser.
include (platform/nk)

# Set compile flags.
project_header_default ("STANDARD_GNU_11:YES" "STRICT_WARNINGS:NO")

add_executable (CrossChecker "src/cross_checker.c")
add_dependencies (CrossChecker traffic_light_idl_files traffic_light_cross_checker_edl_files)