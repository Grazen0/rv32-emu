{
  mkShell,

  rv32-emu,
  xxd,
  riscvPackages,
}:
mkShell {
  hardeningDisable = [
    "relro"
    "bindnow"
  ];

  inputsFrom = [ rv32-emu ];

  packages = [
    riscvPackages.binutils
    riscvPackages.gcc
    riscvPackages.gdb
    xxd
  ];
}
