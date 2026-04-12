#!/bin/bash
# convert everything script

echo "Audio"
python ./audioconv.py galaga_wavetable ../roms/prom-1.1d ../main/arcade_core/games/galaga/galaga_wavetable.h
python ./audioconv.py pacman_wavetable ../roms/82s126.1m ../roms/82s126.3m ../main/arcade_core/games/pacman/pacman_wavetable.h
python ./audioconv.py digdug_wavetable ../roms/136007.110 ../roms/136007.109 ../main/arcade_core/games/digdug/digdug_wavetable.h

echo "Colormaps"
python ./cmapconv.py galaga_colormap_sprites ../roms/prom-5.5n 0 ../roms/prom-3.1c ../main/arcade_core/games/galaga/galaga_cmap_sprites.h
python ./cmapconv.py galaga_colormap_tiles ../roms/prom-5.5n 16 ../roms/prom-4.2n ../main/arcade_core/games/galaga/galaga_cmap_tiles.h
python ./cmapconv.py pacman_colormap ../roms/82s123.7f 0 ../roms/82s126.4a ../main/arcade_core/games/pacman/pacman_cmap.h
python ./cmapconv.py dkong_colormap ../roms/c-2k.bpr ../roms/c-2j.bpr 0 ../roms/v-5e.bpr ../main/arcade_core/games/dkong/dkong_cmap.h
python ./cmapconv.py frogger_colormap ../roms/pr-91.6l ../main/arcade_core/games/frogger/frogger_cmap.h
python ./cmapconv.py digdug_colormap_tiles ../roms/136007.113 0 ../roms/136007.112 ../main/arcade_core/games/digdug/digdug_cmap_tiles.h
python ./cmapconv.py digdug_colormap_sprites ../roms/136007.113 16 ../roms/136007.111 ../main/arcade_core/games/digdug/digdug_cmap_sprites.h
python ./cmapconv.py digdug_colormaps ../roms/136007.113 ../main/arcade_core/games/digdug/digdug_cmap.h
python ./cmapconv.py _1942_colormap_chars ../roms/sb-5.e8,../roms/sb-6.e9,../roms/sb-7.e10 128 ../roms/sb-0.f1 ../main/arcade_core/games/_1942/1942_character_cmap.h
python ./cmapconv.py _1942_colormap_tiles ../roms/sb-5.e8,../roms/sb-6.e9,../roms/sb-7.e10 -1 ../roms/sb-4.d6,../roms/sb-3.d2,../roms/sb-2.d1 ../main/arcade_core/games/_1942/1942_tile_cmap.h
python ./cmapconv.py _1942_colormap_sprites ../roms/sb-5.e8,../roms/sb-6.e9,../roms/sb-7.e10 64 ../roms/sb-8.k3 ../main/arcade_core/games/_1942/1942_sprite_cmap.h

# converted logos are included
#echo "Logos"
#./logoconv.py ../logos/pacman.png ../main/arcade_core/games/pacman/pacman_logo.h
#./logoconv.py ../logos/galaga.png ../main/arcade_core/games/galaga/galaga_logo.h
#./logoconv.py ../logos/dkong.png ../main/arcade_core/games/dkong/dkong_logo.h
#./logoconv.py ../logos/frogger.png ../main/arcade_core/games/frogger/frogger_logo.h
#./logoconv.py ../logos/digdug.png ../main/arcade_core/games/digdug/digdug_logo.h
#./logoconv.py ../logos/1942.png ../main/arcade_core/games/_1942/1942_logo.h

echo "CPU code"
python ./romconv.py -p galaga_rom_cpu1 ../roms/gg1_1b.3p ../roms/gg1_2b.3m ../roms/gg1_3.2m ../roms/gg1_4b.2l ../main/arcade_core/games/galaga/galaga_rom1.h
python ./romconv.py galaga_rom_cpu2 ../roms/gg1_5b.3f ../main/arcade_core/games/galaga/galaga_rom2.h
python ./romconv.py galaga_rom_cpu3 ../roms/gg1_7b.2c ../main/arcade_core/games/galaga/galaga_rom3.h
python ./romconv.py pacman_rom ../roms/pacman.6e ../roms/pacman.6f ../roms/pacman.6h ../roms/pacman.6j ../main/arcade_core/games/pacman/pacman_rom.h
python ./romconv.py dkong_rom_cpu1 ../roms/c_5et_g.bin ../roms/c_5ct_g.bin ../roms/c_5bt_g.bin ../roms/c_5at_g.bin ../main/arcade_core/games/dkong/dkong_rom1.h
python ./romconv.py dkong_rom_cpu2 ../roms/s_3i_b.bin ../roms/s_3j_b.bin ../main/arcade_core/games/dkong/dkong_rom2.h
python ./romconv.py frogger_rom_cpu1 ../roms/frogger.26 ../roms/frogger.27 ../roms/frsm3.7 ../main/arcade_core/games/frogger/frogger_rom1.h
python ./romconv.py frogger_rom_cpu2 ../roms/frogger.608 ../roms/frogger.609 ../roms/frogger.610 ../main/arcade_core/games/frogger/frogger_rom2.h
python ./romconv.py digdug_rom_cpu1 ../roms/dd1a.1 ../roms/dd1a.2 ../roms/dd1a.3 ../roms/dd1a.4 ../main/arcade_core/games/digdug/digdug_rom1.h
python ./romconv.py digdug_rom_cpu2 ../roms/dd1a.5 ../roms/dd1a.6 ../main/arcade_core/games/digdug/digdug_rom2.h
python ./romconv.py digdug_rom_cpu3 ../roms/dd1.7 ../main/arcade_core/games/digdug/digdug_rom3.h
python ./romconv.py digdug_playfield ../roms/dd1.10b ../main/arcade_core/games/digdug/digdug_playfield.h
python ./romconv.py _1942_rom_cpu1 ../roms/srb-03.m3 ../roms/srb-04.m4 ../main/arcade_core/games/_1942/1942_rom1.h
python ./romconv.py _1942_rom_cpu1_b0 ../roms/srb-05.m5 ../main/arcade_core/games/_1942/1942_rom1_b0.h
python ./romconv.py _1942_rom_cpu1_b1 ../roms/srb-06.m6 ../main/arcade_core/games/_1942/1942_rom1_b1.h
python ./romconv.py _1942_rom_cpu1_b2 ../roms/srb-07.m7 ../main/arcade_core/games/_1942/1942_rom1_b2.h
python ./romconv.py _1942_rom_cpu2 ../roms/sr-01.c11 ../main/arcade_core/games/_1942/1942_rom2.h

echo "Sprites"
python ./spriteconv.py galaga_sprites galaga ../roms/gg1_11.4d ../roms/gg1_10.4f ../main/arcade_core/games/galaga/galaga_spritemap.h
python ./spriteconv.py pacman_sprites pacman ../roms/pacman.5f ../main/arcade_core/games/pacman/pacman_spritemap.h
python ./spriteconv.py dkong_sprites dkong ../roms/l_4m_b.bin  ../roms/l_4n_b.bin  ../roms/l_4r_b.bin  ../roms/l_4s_b.bin ../main/arcade_core/games/dkong/dkong_spritemap.h
python ./spriteconv.py frogger_sprites frogger ../roms/frogger.606 ../roms/frogger.607 ../main/arcade_core/games/frogger/frogger_spritemap.h
python ./spriteconv.py digdug_sprites digdug ../roms/dd1.15 ../roms/dd1.14 ../roms/dd1.13 ../roms/dd1.12 ../main/arcade_core/games/digdug/digdug_spritemap.h
python ./spriteconv.py _1942_sprites 1942 ../roms/sr-14.l1 ../roms/sr-15.l2 ../roms/sr-16.n1 ../roms/sr-17.n2 ../main/arcade_core/games/_1942/1942_spritemap.h

# converted starset is included
#echo "Starset"
#./starsets.py ../main/arcade_core/games/galaga/galaga_starseed.h

# converted tile address map is included
#echo "Tile address map"
#./tileaddr.py ../main/arcade_core/tileaddr.h

echo "Tiles"
python ./tileconv.py ../roms/gg1_9.4l ../main/arcade_core/games/galaga/galaga_tilemap.h
python ./tileconv.py ../roms/pacman.5e ../main/arcade_core/games/pacman/pacman_tilemap.h
python ./tileconv.py ../roms/v_5h_b.bin ../roms/v_3pt.bin ../main/arcade_core/games/dkong/dkong_tilemap.h
python ./tileconv.py ../roms/frogger.606 ../roms/frogger.607 ../main/arcade_core/games/frogger/frogger_tilemap.h
python ./tileconv.py ../roms/dd1.9 ../main/arcade_core/games/digdug/digdug_tilemap.h
python ./tileconv.py ../roms/dd1.11 ../main/arcade_core/games/digdug/digdug_pftiles.h
python ./tileconv.py ../roms/sr-02.f2 ../main/arcade_core/games/_1942/1942_charmap.h
python ./tileconv.py ../roms/sr-08.a1 ../roms/sr-09.a2 ../roms/sr-10.a3 ../roms/sr-11.a4 ../roms/sr-12.a5 ../roms/sr-13.a6 ../main/arcade_core/games/_1942/1942_tilemap.h

echo "Z80"
python ./z80patch.py


