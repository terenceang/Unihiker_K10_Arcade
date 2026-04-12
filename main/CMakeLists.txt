set(srcs
        "main.cpp"
        "state/k10_state.cpp"
        "emulator/k10_emulator.cpp"
        "emulator/k10_emulator_core.c"
        "hardware/k10_hardware.cpp"
        "hardware/k10_video.cpp"
        "state/k10_wifi_gamepad.cpp")

set(requires "")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "." "arcade_core" "config" "hardware" "emulator" "state" "gamepad"
                    REQUIRES "${requires}"
)
set_property(TARGET ${COMPONENT_LIB} PROPERTY embed_txtfiles "gamepad/gamepad.html")
