{
  description = "A quick and dirty RISC-V 32 CPU emulator written in C.";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
    flake-parts.url = "github:hercules-ci/flake-parts";
    systems.url = "github:nix-systems/default";
  };

  outputs =
    inputs@{
      self,
      flake-parts,
      ...
    }:
    flake-parts.lib.mkFlake { inherit inputs; } {
      systems = import inputs.systems;

      perSystem =
        {
          self',
          pkgs,
          system,
          ...
        }:
        {
          packages = {
            rv32-emu = pkgs.callPackage ./default.nix { };
            default = self'.packages.rv32-emu;
          };

          devShells.default = pkgs.callPackage ./shell.nix {
            inherit (self'.packages) rv32-emu;
          };
        };
    };
}
