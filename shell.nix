{
  mkShell,

  rv32-emu,
  xxd,
}:
mkShell {
  inputsFrom = [ rv32-emu ];

  packages = [
    xxd
  ];
}
