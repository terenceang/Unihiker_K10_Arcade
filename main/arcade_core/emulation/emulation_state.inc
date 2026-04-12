extern void StepZ80(Z80 *R);
extern void digdug_StepZ80(Z80 *R);

Z80 cpu[3];    // up to three z80 supported

unsigned char *memory = NULL;

char game_started = 0;
char current_cpu = 0;
const unsigned char *current_rom_base = NULL;
char irq_enable[3] = { 0,0,0 };

#if defined(ENABLE_GALAGA) || defined(ENABLE_DIGDUG) || defined(ENABLE_1942)
char sub_cpu_reset = 1;
#endif

#ifdef ENABLE_GALAGA
// special variables for galaga
unsigned char credit = 0;
char credit_mode = 0;
int namco_cnt = 0;
int namco_busy = 0;
unsigned char cs_ctrl = 0;
int nmi_cnt = 0;
int coincredMode = 0;
unsigned char starcontrol = 0;
#endif

#ifdef ENABLE_PACMAN
// special variables for pacman
unsigned char irq_ptr = 0;
#endif

#ifdef ENABLE_DKONG
// special variables for dkong
unsigned char colortable_select = 0;

// the audio cpu is a mb8884 which in turn is 8048/49 compatible
struct i8048_state_S cpu_8048;

// sound effects register between 8048 and z80
unsigned char dkong_sfx_index = 0x00;
#endif

#ifdef ENABLE_FROGGER
unsigned char frogger_snd_irq_state = 0;
unsigned char frogger_snd_command;
unsigned char frogger_snd_ay_port;
unsigned long frogger_snd_icnt;
#endif

#ifdef ENABLE_DIGDUG
unsigned char namco_command = 0;
unsigned char namco_mode = 0;
unsigned char namco_nmi_counter = 0;
unsigned char namco_credit = 0x00;
unsigned char digdug_video_latch = 0x00;
#define NAMCO_NMI_DELAY  30  // 10 results in errors
#endif

#ifndef SINGLE_MACHINE
signed char machine = MCH_MENU;

// if galaga is installed, then use it as default in menu
#ifdef ENABLE_GALAGA
signed char menu_sel = MCH_GALAGA;
#else
// otherwise just use the first game
signed char menu_sel = 1;
#endif

#ifdef MASTER_ATTRACT_MENU_TIMEOUT
// menu timeout for master attract mode which randomly start games
unsigned long master_attract_timeout = 0;
#endif
#endif

#if defined(ENABLE_PACMAN) || defined(ENABLE_GALAGA) || defined(ENABLE_FROGGER) || defined(ENABLE_DIGDUG) || defined(ENABLE_1942)
// mirror of sounds registers
unsigned char soundregs[32] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};
#endif
