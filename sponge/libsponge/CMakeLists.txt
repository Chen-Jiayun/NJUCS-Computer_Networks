file (GLOB LIB_SOURCES "*.cc" "util/*.cc" "tcp_helpers/*.cc")
# fix stupid hint in vscode
include_directories("./tcp_helpers")
add_library (sponge STATIC ${LIB_SOURCES})
