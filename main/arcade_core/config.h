#ifndef _CONFIG_H_
#define _CONFIG_H_

#include "k10_config.h"

// disable e.g. if roms are missing
// #define ENABLE_PACMAN
// #define ENABLE_GALAGA
// #define ENABLE_DKONG
 #define ENABLE_FROGGER
// #define ENABLE_DIGDUG
//#define ENABLE_1942

#ifdef ENABLE_PACMAN
#define K10_ENABLE_COUNT_PACMAN 1
#else
#define K10_ENABLE_COUNT_PACMAN 0
#endif

#ifdef ENABLE_GALAGA
#define K10_ENABLE_COUNT_GALAGA 1
#else
#define K10_ENABLE_COUNT_GALAGA 0
#endif

#ifdef ENABLE_DKONG
#define K10_ENABLE_COUNT_DKONG 1
#else
#define K10_ENABLE_COUNT_DKONG 0
#endif

#ifdef ENABLE_FROGGER
#define K10_ENABLE_COUNT_FROGGER 1
#else
#define K10_ENABLE_COUNT_FROGGER 0
#endif

#ifdef ENABLE_DIGDUG
#define K10_ENABLE_COUNT_DIGDUG 1
#else
#define K10_ENABLE_COUNT_DIGDUG 0
#endif

#ifdef ENABLE_1942
#define K10_ENABLE_COUNT_1942 1
#else
#define K10_ENABLE_COUNT_1942 0
#endif

#define K10_ENABLED_MACHINE_COUNT \
  (K10_ENABLE_COUNT_PACMAN + K10_ENABLE_COUNT_GALAGA + K10_ENABLE_COUNT_DKONG + \
   K10_ENABLE_COUNT_FROGGER + K10_ENABLE_COUNT_DIGDUG + K10_ENABLE_COUNT_1942)

#if K10_ENABLED_MACHINE_COUNT == 0
#error "At least one machine has to be enabled!"
#endif

// check if only one machine is enabled
#if K10_ENABLED_MACHINE_COUNT == 1
  #define SINGLE_MACHINE
#endif

// game config
#define MASTER_ATTRACT_MENU_TIMEOUT  20000   // start games randomly while sitting idle in menu for 20 seconds, undefine to disable

#include "dip_switches.h"

// Aliases for k10_config values used in the emulator core
#define AUDIO_VOLUME   K10_AUDIO_VOLUME
#define LED_PIN        K10_BIT_USER_LED
#define LED_BRIGHTNESS K10_LED_BRIGHTNESS

#endif // _CONFIG_H_
