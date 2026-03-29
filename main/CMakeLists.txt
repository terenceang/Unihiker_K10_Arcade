set(srcs
        "main.cpp"
        "k10_state.cpp"
        "k10_emulator.cpp"
        "k10_emulator_core.c"
        "k10_hardware.cpp"
        "k10_video.cpp"
        "k10_wifi_gamepad.cpp")

set(requires "")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "." "arcade_core"
                    REQUIRES "${requires}"
)
set_property(TARGET ${COMPONENT_LIB} PROPERTY embed_txtfiles "gamepad.html")
