{
  mkShell,

  rv32-emu,
  xxd,
  riscvPackages,
}:
mkShell {
  inputsFrom = [ rv32-emu ];

  packages = [
    riscvPackages.binutils
    riscvPackages.gcc
    riscvPackages.gdb
    xxd
  ];
}
