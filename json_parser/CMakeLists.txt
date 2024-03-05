project (json-parser)

# Set compile flags.
project_header_default ("STANDARD_GNU_11:YES" "STRICT_WARNINGS:NO")

add_library(json_parser "json.c")

target_link_libraries (json_parser m)