project (LightsGPIO)

# Tools for using NK parser.
include (platform/nk)

# Set compile flags.
project_header_default ("STANDARD_GNU_11:YES" "STRICT_WARNINGS:NO")

add_executable (LightsGPIO1 "src/lights_gpio.c")
add_dependencies (LightsGPIO1 traffic_light_lights_gpio_edl_files)

add_executable (LightsGPIO2 "src/lights_gpio.c")
add_dependencies (LightsGPIO2 traffic_light_lights_gpio_edl_files)
