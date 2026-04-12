# Samples

Some arcade machines use analog or discrete sound hardware that is easier to
reproduce as short digital samples on ESP32.

This folder contains .s8 source samples that are converted into C headers for
the emulator core.

## Input Files

- galaga_boom.s8
- dkong_walk0.s8
- dkong_walk1.s8
- dkong_walk2.s8
- dkong_jump.s8
- dkong_stomp.s8

## Output Location

Generated headers should be written to:

- ../main/arcade_core/games/galaga/
- ../main/arcade_core/games/dkong/

## Convert Commands

Run these commands from the samples directory.

### Galaga

python ../romconv/romconv.py galaga_sample_boom ./galaga_boom.s8 ../main/arcade_core/games/galaga/galaga_sample_boom.h

### Donkey Kong

python ../romconv/romconv.py dkong_sample_walk0 ./dkong_walk0.s8 ../main/arcade_core/games/dkong/dkong_sample_walk0.h
python ../romconv/romconv.py dkong_sample_walk1 ./dkong_walk1.s8 ../main/arcade_core/games/dkong/dkong_sample_walk1.h
python ../romconv/romconv.py dkong_sample_walk2 ./dkong_walk2.s8 ../main/arcade_core/games/dkong/dkong_sample_walk2.h
python ../romconv/romconv.py dkong_sample_jump  ./dkong_jump.s8  ../main/arcade_core/games/dkong/dkong_sample_jump.h
python ../romconv/romconv.py dkong_sample_stomp ./dkong_stomp.s8 ../main/arcade_core/games/dkong/dkong_sample_stomp.h

## Notes

- Donkey Kong uses an MB8884/I8048 audio CPU for most sounds.
- The listed DK samples are for effects generated via discrete logic.
