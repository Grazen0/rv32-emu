{
  stdenv,
  lib,
  cmake,
  pkg-config,
  unity-test,
  ruby,
}:
stdenv.mkDerivation (finalAttrs: {
  pname = "rv32-emu";
  version = "main";

  src = lib.cleanSource ./.;

  nativeBuildInputs = [
    cmake
    pkg-config
    unity-test
    ruby
  ];

  cmakeFlags = [
    (lib.cmakeBool "BUILD_TESTING" finalAttrs.doCheck)
  ];

  enableParallelBuilding = true;
  doCheck = true;

  meta = with lib; {
    description = "A quick and dirty RISC-V 32 CPU emulator written in C.";
    homepage = "https://github.com/Grazen0/rv32-emu";
    license = licenses.gpl3;
  };
})
