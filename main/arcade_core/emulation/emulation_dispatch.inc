void OutZ80(unsigned short Port, unsigned char Value) {
#ifdef ENABLE_PACMAN
  if(MACHINE_IS_PACMAN) {
    pacman_OutZ80(Port, Value);
    return;
  }
#endif

#ifdef ENABLE_FROGGER
  if(MACHINE_IS_FROGGER) {
    frogger_OutZ80(Port, Value);
    return;
  }
#endif
}

unsigned char InZ80(unsigned short Port) {
#ifdef ENABLE_FROGGER
  if(MACHINE_IS_FROGGER)
    return frogger_InZ80(Port);
#endif

      return 0;
}

// Memory write -- write the Value to memory location Addr
void WrZ80(unsigned short Addr, unsigned char Value) {
#if 0
  // jump table
  const void (*wrz80[])(unsigned short, unsigned char) = {
     0,
#ifdef ENABLE_PACMAN
     pacman_WrZ80,
#endif
#ifdef ENABLE_GALAGA
     galaga_WrZ80,
#endif
#ifdef ENABLE_DKONG
     dkong_WrZ80,
#endif
#ifdef ENABLE_FROGGER
     frogger_WrZ80,
#endif
#ifdef ENABLE_DIGDUG
     digdug_WrZ80,
#endif
#ifdef ENABLE_1942
     _1942_WrZ80,
#endif
    };

  (*(wrz80[machine]))(Addr, Value);

#else
      // Dig Dug is very timing critical and gets a special
  // treatment
#ifdef ENABLE_DIGDUG
  if(MACHINE_IS_DIGDUG) {
    digdug_WrZ80(Addr, Value);
    return;
  }
#endif

#ifndef SINGLE_MACHINE
  switch(machine) {
#endif

#ifdef ENABLE_PACMAN
#ifndef SINGLE_MACHINE
    case MCH_PACMAN:
#endif
      pacman_WrZ80(Addr, Value);
      return;
#endif
#ifndef SINGLE_MACHINE
      break;
#endif

#ifdef ENABLE_GALAGA
#ifndef SINGLE_MACHINE
    case MCH_GALAGA:
#endif
      galaga_WrZ80(Addr, Value);
      return;
#endif
#ifndef SINGLE_MACHINE
      break;
#endif

#ifdef ENABLE_DKONG
#ifndef SINGLE_MACHINE
    case MCH_DKONG:
#endif
      dkong_WrZ80(Addr, Value);
      return;
#endif
#ifndef SINGLE_MACHINE
      break;
#endif

#ifdef ENABLE_FROGGER
#ifndef SINGLE_MACHINE
    case MCH_FROGGER:
#endif
      frogger_WrZ80(Addr, Value);
      return;
#endif
#ifndef SINGLE_MACHINE
      break;
#endif

#ifdef ENABLE_DIGDUG
#ifndef SINGLE_MACHINE
    case MCH_DIGDUG:
#endif
      digdug_WrZ80(Addr, Value);
      return;
#endif
#ifndef SINGLE_MACHINE
      break;
#endif

#ifdef ENABLE_1942
#ifndef SINGLE_MACHINE
    case MCH_1942:
#endif
      _1942_WrZ80(Addr, Value);
      return;
#endif
#ifndef SINGLE_MACHINE
      break;
  }
#endif
#endif
}

unsigned char RdZ80(unsigned short Addr) {
#if 0
  // jump table
  const unsigned char (*rdz80[])(unsigned short) = {
     0,
#ifdef ENABLE_PACMAN
     pacman_RdZ80,
#endif
#ifdef ENABLE_GALAGA
     galaga_RdZ80,
#endif
#ifdef ENABLE_DKONG
     dkong_RdZ80,
#endif
#ifdef ENABLE_FROGGER
     frogger_RdZ80,
#endif
#ifdef ENABLE_DIGDUG
     digdug_RdZ80,
#endif
#ifdef ENABLE_1942
     _1942_RdZ80,
#endif
    };
  return (*(rdz80[machine]))(Addr);

#else

      // Fast treatment for Dig Dug as it's quite
  // performance sensitive
#ifdef ENABLE_DIGDUG
  if(MACHINE_IS_DIGDUG) return digdug_RdZ80(Addr);
#endif

#ifndef SINGLE_MACHINE
  switch(machine) {
#endif

#ifdef ENABLE_PACMAN
#ifndef SINGLE_MACHINE
    case MCH_PACMAN:
#endif
      return pacman_RdZ80(Addr);
#endif
#ifndef SINGLE_MACHINE
      break;
#endif

#ifdef ENABLE_GALAGA
#ifndef SINGLE_MACHINE
    case MCH_GALAGA:
#endif
      return galaga_RdZ80(Addr);
#endif
#ifndef SINGLE_MACHINE
      break;
#endif

#ifdef ENABLE_DKONG
#ifndef SINGLE_MACHINE
    case MCH_DKONG:
#endif
      return dkong_RdZ80(Addr);
#endif
#ifndef SINGLE_MACHINE
      break;
#endif

#ifdef ENABLE_FROGGER
#ifndef SINGLE_MACHINE
    case MCH_FROGGER:
#endif
      return frogger_RdZ80(Addr);
#endif
#ifndef SINGLE_MACHINE
      break;
#endif

#ifdef ENABLE_DIGDUG
#ifndef SINGLE_MACHINE
    case MCH_DIGDUG:
#endif
      return digdug_RdZ80(Addr);
#endif
#ifndef SINGLE_MACHINE
      break;
#endif

#ifdef ENABLE_1942
#ifndef SINGLE_MACHINE
    case MCH_1942:
#endif
      return _1942_RdZ80(Addr);
#endif
#ifndef SINGLE_MACHINE
      break;
  }
  return 0xff;
#endif
#endif
}
