cmake_minimum_required(VERSION 3.22)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(COMPONENTS main)
# Only the hardware/network-free components, added directly (not the whole
# ../components dir): the component manager resolves every idf_component.yml
# it finds under EXTRA_COMPONENT_DIRS even for components nothing requires,
# and web_server's manifest pulls in espressif/mdns, which doesn't support
# IDF_TARGET=linux.
list(APPEND EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../components/framebuffer"
    "${CMAKE_CURRENT_LIST_DIR}/../components/web_content"
    "${CMAKE_CURRENT_LIST_DIR}/../components/dir_scan"
)

project(ludo001_host_tests)
