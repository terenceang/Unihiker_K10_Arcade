/* Linker script fragment to embed gamepad.html as binary for ESP-IDF */
/* Place in main/CMakeLists.txt: target_link_libraries(${COMPONENT_LIB} INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/gamepad_html.ld") */

INCLUDE "gamepad.html"
