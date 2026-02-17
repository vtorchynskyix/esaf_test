{
  inputs = {
    nixpkgs.url = "nixpkgs";
    unstable.url = "nixpkgs/nixos-unstable";
  };

  outputs = {self, nixpkgs, unstable, ...}:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs { system = system; };
      upkgs = import unstable { system = system; };
    in
    {
      inherit pkgs;

      devShell.${system} = upkgs.mkShell {

        nixFlakeName="zhalo:";
        packages = [
          upkgs.gnumake42 upkgs.stlink upkgs.stm32cubemx pkgs.gcc-arm-embedded-12 upkgs.openocd upkgs.vscode
        ];

        src = self;
        shellHook = ''
        export top=$(pwd)
        echo top=$top
        #        make -C Debug zhalo1.elf
        . $top/.rc

        '';
        # shit will appear in ~/STM32Cube/Repository

      };
    };
}
