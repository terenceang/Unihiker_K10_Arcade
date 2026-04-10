# 0 "temp_compile_test.c"
# 0 "<built-in>"
# 0 "<command-line>"
# 1 "temp_compile_test.c"

# 1 "main/arcade_core/digdug.h" 1




# 1 "main/arcade_core/digdug_rom1.h" 1
const unsigned char digdug_rom_cpu1[] = {
# 6 "main/arcade_core/digdug.h" 2
# 1 "main/arcade_core/digdug_rom2.h" 1
const unsigned char digdug_rom_cpu2[] = {
# 7 "main/arcade_core/digdug.h" 2
# 1 "main/arcade_core/digdug_rom3.h" 1
const unsigned char digdug_rom_cpu3[] = {
# 8 "main/arcade_core/digdug.h" 2

static inline unsigned char digdug_RdZ80(unsigned short Addr) {
  static const unsigned char *rom[] = { digdug_rom_cpu1, digdug_rom_cpu2, digdug_rom_cpu3 };

  if(Addr < 16384)
    return rom[current_cpu][Addr];


  if((Addr & 0xe000) == 0x8000)
    return memory[Addr - 0x8000];


  if((Addr & 0xfe00) == 0x7000)
    return namco_read_dd(Addr & 0x1ff);




  if((Addr & 0xffc0) == 0xb800)
    return 0x00;

  return 0xff;
}

static inline void digdug_WrZ80(unsigned short Addr, unsigned char Value) {
  if((Addr & 0xe000) == 0x8000) {
# 47 "main/arcade_core/digdug.h"
    if(Addr == 0x8000 + 985 && (Value & 0x7f) == 46)
      game_started = 1;

    memory[Addr - 0x8000] = Value;
    return;
  }

  if((Addr & 0xffe0) == 0x6800) {
    soundregs[Addr - 0x6800] = Value & 0x0f;
    return;
  }

  if((Addr & 0xfff8) == 0x6820) {
    if((Addr & 0x0c) == 0x00) {
      if((Addr & 3) < 3) {
       irq_enable[Addr & 3] = Value & 1;
      } else {
       sub_cpu_reset = !Value;

       if(sub_cpu_reset) {

         namco_command = 0x00;
         namco_mode = 0;
         namco_nmi_counter = 0;

         current_cpu = 1; ResetZ80(&cpu[1]);
         current_cpu = 2; ResetZ80(&cpu[2]);
       }
      }
    }
    return;
  }



  if((Addr & 0xfe00) == 0x7000) {
    namco_write_dd(Addr & 0x1ff, Value);
    return;
  }


  if((Addr & 0xfff8) == 0xa000) {
    if(Value & 1) digdug_video_latch |= (1<<(Addr & 7));
    else digdug_video_latch &= ~(1<<(Addr & 7));
    return;
  }






}

static inline void digdug_run_frame(void) {
  for(char c=0;c<4;c++) {
    for(int i=0;i<INST_PER_FRAME/4;i++) {
      current_cpu = 0;
      StepZ80(cpu); digdug_StepZ80(cpu); digdug_StepZ80(cpu); digdug_StepZ80(cpu);
      if(!sub_cpu_reset) {

        current_cpu = 1;
        StepZ80(cpu+1); digdug_StepZ80(cpu+1); digdug_StepZ80(cpu+1); digdug_StepZ80(cpu+1);
        current_cpu = 2;
        StepZ80(cpu+2); digdug_StepZ80(cpu+2); digdug_StepZ80(cpu+2); digdug_StepZ80(cpu+2);
      }


      if(namco_nmi_counter) {
        namco_nmi_counter--;
        if(!namco_nmi_counter) {
          current_cpu = 0;
          IntZ80(&cpu[0], INT_NMI);
          namco_nmi_counter = NAMCO_NMI_DELAY;
        }
      }
    }


    if(!sub_cpu_reset && !irq_enable[2] && ((c == 1) || (c == 3))) {
      current_cpu = 2;
      IntZ80(&cpu[2], INT_NMI);
    }
  }

  if(irq_enable[0]) {
    current_cpu = 0;
    IntZ80(&cpu[0], INT_RST38);
  }

  if(!sub_cpu_reset && irq_enable[1]) {
    current_cpu = 1;
    IntZ80(&cpu[1], INT_RST38);
  }
}
# 3 "temp_compile_test.c" 2
int main(void){return 0;}
