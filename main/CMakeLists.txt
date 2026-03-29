set(srcs
        "main.cpp"
        "k10_state.cpp"
        "k10_emulator.cpp"
        "k10_emulator_core.c"
        "k10_hardware.cpp"
        "k10_video.cpp")

set(requires "")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "." "arcade_core"
                    REQUIRES "${requires}")
