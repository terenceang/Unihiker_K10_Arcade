#ifdef ENABLE_DIGDUG
void namco_write_dd(unsigned short Addr, unsigned char Value) {
  if(Addr & 0x100) {
    namco_command = Value;
    if(Value == 0xa1) namco_mode = 1;
    if((Value == 0xc1) || (Value == 0xe1)) namco_mode = 0;

    // Any command other than 0x10 triggers an NMI to the main CPU.
    if(Value != 0x10) namco_nmi_counter = NAMCO_NMI_DELAY;
    else              namco_nmi_counter = 0;
  }
}

unsigned char namco_read_dd(unsigned short Addr) {
  if(Addr & 0x100)
    return namco_command;
  else {
    unsigned char keymask = buttons_get();

    static unsigned char keymask_d[] = { 0x00, 0x00, 0x00 };
    keymask_d[2] = keymask_d[1];
    keymask_d[1] = keymask_d[0];
    keymask_d[0] = keymask;

    // Rising edge, prolonged for two calls.
    unsigned char keymask_p = (keymask ^ keymask_d[2]) & keymask;

    if((keymask_d[0] & BUTTON_COIN) && !(keymask_d[1] & BUTTON_COIN) && (namco_credit < 99))
      namco_credit++;

    // This decrease of credit actually triggers the game start.
    if((keymask_d[0] & BUTTON_START) && !(keymask_d[1] & BUTTON_START) && (namco_credit > 0))
      namco_credit--;

    if(namco_command == 0x71) {
      if(namco_mode) {
        if((Addr & 15) == 0) {
          unsigned char retval = 0x00;
          if(keymask & BUTTON_COIN)   retval |= 0x01;
          if(keymask & BUTTON_START)  retval |= 0x10;
          return ~retval;
        } else if((Addr & 15) == 1) {
          unsigned char retval = 0x00;
          if(keymask & BUTTON_UP)        retval |= 0x01;
          if(keymask & BUTTON_DOWN)      retval |= 0x04;
          if(keymask & BUTTON_LEFT)      retval |= 0x08;
          if(keymask & BUTTON_RIGHT)     retval |= 0x02;
          if(keymask_d[1] & BUTTON_FIRE) retval |= 0x20;
          if(keymask_p & BUTTON_FIRE)    retval |= 0x10;
          return ~retval;
        } else if((Addr & 15) == 2)
          return ~0b00000000;
      } else {
        if((Addr & 15) == 0) {
          return 16*(namco_credit/10) + namco_credit % 10;
        } else if((Addr & 15) == 1) {
          unsigned char retval = 0x00;
          // first player controls
          if(keymask & BUTTON_FIRE)       retval |= 0x20;
          if(keymask_p & BUTTON_FIRE)     retval |= 0x10;

          if(keymask & BUTTON_UP)         retval |= 0x0f;
          else if(keymask & BUTTON_RIGHT) retval |= 0x0d;
          else if(keymask & BUTTON_DOWN)  retval |= 0x0b;
          else if(keymask & BUTTON_LEFT)  retval |= 0x09;
          else                            retval |= 0x07;   // nothing

          return ~retval;
        } else if((Addr & 15) == 2)
          return 0xf8;  // f8 -> no second player control
      }

    } else if(namco_command == 0xd2) {

      if((Addr & 15) == 0)
        // DSW0
        return ~DIGDUG_DIP_A;
      else if((Addr & 15) == 1)
        // DSW1
        return (unsigned char)~DIGDUG_DIP_B;

    } else if(namco_command == 0xb1) {
      // {8{~(ADR<=2)}};
      if((Addr & 15) <= 2) return 0x00;
      return 0xff;
    } else if(namco_command == 0x08) {
      return 0xff;
    } else if(namco_command == 0xc1)
      return 0xff;
  }
  return 0xff;
}
#endif

#ifdef ENABLE_DKONG
static unsigned char dkong_audio_assembly_buffer[64];
unsigned char dkong_audio_transfer_buffer[DKONG_AUDIO_QUEUE_LEN][64];
unsigned char dkong_audio_rptr = 0, dkong_audio_wptr = 0;

void i8048_port_write(struct i8048_state_S *state, unsigned char port, unsigned char pos) {
  if(port == 0)
    return;

  else if(port == 1) {
    static int bptr = 0;

    dkong_audio_assembly_buffer[bptr++] = pos;

    // buffer now full?
    if(bptr == 64) {
      bptr = 0;

      // It must never happen that we get here with no free transfer buffers
      // available. This would mean that the buffers were full and the
      // 8048 emulation was still running. It should be stopped as long as the
      // buffers are full.
      if(((dkong_audio_wptr + 1)&DKONG_AUDIO_QUEUE_MASK) == dkong_audio_rptr) {
        // overflow
      } else {
        // copy data into transfer buffer
        memcpy(dkong_audio_transfer_buffer[dkong_audio_wptr], dkong_audio_assembly_buffer, 64);
        dkong_audio_wptr = (dkong_audio_wptr + 1)&DKONG_AUDIO_QUEUE_MASK;
      }
    }
  } else if(port == 2)
    state->p2_state = pos;
}

unsigned char i8048_port_read(struct i8048_state_S *state, unsigned char port) {
  if(port == 2) return state->p2_state;
  return 0;
}

void i8048_xdm_write(struct i8048_state_S *state, unsigned char addr, unsigned char data) { }

unsigned char i8048_xdm_read(struct i8048_state_S *state, unsigned char addr) {
  // inverted Z80 MUS register
  if(state->p2_state & 0x40)
    return dkong_sfx_index ^ 0x0f;

  return dkong_rom_cpu2[2048 + addr + 256 * (state->p2_state & 7)];
}

// this is inlined in emulation.h
//unsigned char i8048_rom_read(struct i8048_state_S *state, unsigned short addr) {
//  return dkong_rom_cpu2[addr];
//}
#endif
