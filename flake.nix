{
  description = "UsdSolid BrepArray and OCCT bridge";
  inputs = {
    aeco-toolchain.url = "github:criad-com/aeco-toolchain?ref=34a29f51a232ac77d5e9f55431e537c426724035";
    usdaeco-toolchain.url = "github:criad-com/usdaeco-toolchain?ref=v0.3.9";
    usdaeco-toolchain.inputs.aeco-toolchain.follows = "aeco-toolchain";
    nixpkgs.follows = "aeco-toolchain/nixpkgs";
    usdSolid.url = "github:criad-com/usdSolid?ref=v0.1.0";
    usdSolid.inputs.aeco-toolchain.follows = "aeco-toolchain";
    usdSolid.inputs.usdaeco-toolchain.follows = "usdaeco-toolchain";
    upstream = {
      url = "github:jensjebens/OpenUSD?rev=d618f8ac62cefa02f6765d8e3784ea503195ce86";
      flake = false;
    };
  };
  outputs = { self, nixpkgs, aeco-toolchain, usdaeco-toolchain, usdSolid, upstream }:
    let
      eachSystem = nixpkgs.lib.genAttrs [ "aarch64-darwin" "x86_64-linux" ];
      forSystem = system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
          kit = usdaeco-toolchain.lib.forSystem system;
          solid = usdSolid.packages.${system};
          occt = aeco-toolchain.packages.${system}.occt;
          bridge = usdaeco-toolchain.lib.buildNativePlugin {
            inherit system;
            name = "usdSolidOcct";
            src = pkgs.lib.fileset.toSource {
              root = ./.;
              fileset = pkgs.lib.fileset.unions [ ./CMakeLists.txt ./library.json
                ./plugInfo.json.in ./LICENSE ./usdSolidOcct ./tools/extract.py
                ./tools/source_manifest.py ./tools/probe.py ./tools/check_support.py
                ./testenv/testUsdSolidOcct.cpp ./testenv/fixtures.cpp ./testenv/smoke.py
                ./testenv/primitives.py ./testenv/contracts.py ./testenv/diagnose.cpp ];
            };
            deps = [ solid.usdSolid occt ];
            cmakeFlags = [
              "-DUSDSOLIDOCCT_UPSTREAM=${upstream}"
              "-DUSDSOLIDOCCT_FIXTURES=${solid.fixtures}"
              "-DUSDSOLIDOCCT_PLUGINS=${solid.pluginSet}"
              "-DUSDSOLIDOCCT_SCHEMA_PYTHON=${solid.usdSolid}/${pkgs.python3.sitePackages}"
            ];
          };
          pythonModules = pkgs.python3.pkgs.toPythonModule (pkgs.runCommand "usdSolidOcct-python" {} ''
            mkdir -p "$out/${pkgs.python3.sitePackages}/pxr"
            ln -s ${solid.usdSolid}/${pkgs.python3.sitePackages}/pxr/UsdSolid "$out/${pkgs.python3.sitePackages}/pxr/UsdSolid"
            ln -s ${bridge}/${pkgs.python3.sitePackages}/pxr/UsdSolidOcct "$out/${pkgs.python3.sitePackages}/pxr/UsdSolidOcct"
          '');
          pythonEnv = pkgs.python3.withPackages (ps: [ kit.usdPython pythonModules ps.pytest ps.packaging ]);
          pluginSet = kit.pluginSet { plugins = [ bridge solid.usdSolidValidators ]; };
          consumer = pkgs.stdenv.mkDerivation {
            name = "usdSolidOcct-installed-consumer";
            src = ./testenv/consumer;
            nativeBuildInputs = [ pkgs.cmake pkgs.ninja ];
            buildInputs = [ bridge pkgs.python3 ];
            doCheck = true;
            checkPhase = "ctest --output-on-failure";
          };
          runtime = pkgs.runCommand "usdSolidOcct-runtime" {} ''
            mkdir -p "$out"
            cat > "$out/paths.json" <<'JSON'
            ${builtins.toJSON {
              bridge = "${bridge}";
              schema = "${solid.usdSolid}";
              validators = "${solid.usdSolidValidators}";
              python = "${pythonEnv}/bin/python";
              plugins = "${pluginSet}";
              fixtures = "${solid.fixtures}";
              toolchain = "${usdaeco-toolchain}";
              upstream = "${upstream}";
              occt = "${occt}";
              consumer = "${consumer}";
              closure = "${pkgs.closureInfo { rootPaths = [ bridge ]; }}/store-paths";
              revisions = {
                aeco-toolchain = aeco-toolchain.rev;
                usdaeco-toolchain = usdaeco-toolchain.rev;
                usdSolid = usdSolid.rev;
                upstream = upstream.rev;
              };
            }}
            JSON
          '';
        in { inherit pkgs bridge pythonEnv pluginSet runtime consumer; };
    in {
      packages = eachSystem (system: let p = forSystem system; in {
        inherit (p) pythonEnv pluginSet runtime consumer;
        usdSolidOcct = p.bridge;
        default = p.bridge;
      });
      checks = eachSystem (system: let p = forSystem system; in {
        native = p.bridge;
        installed-consumer = p.consumer;
        acceptance = p.pkgs.runCommand "usdSolidOcct-acceptance" {} ''
          cp -R ${self} source
          chmod -R u+w source
          cd source
          export USD_SOLID_OCCT_RUNTIME=${p.runtime}
          env -u PYTHONPATH ${p.pythonEnv}/bin/python check.py
          env -u PYTHONPATH ${p.pythonEnv}/bin/python -m pytest -q
          touch "$out"
        '';
      });
      devShells = eachSystem (system: let p = forSystem system; in {
        default = p.pkgs.mkShell {
          inputsFrom = [ aeco-toolchain.devShells.${system}.native ];
          packages = [ p.pythonEnv ];
          shellHook = ''
            unset PYTHONPATH
            export USD_SOLID_OCCT_RUNTIME=${p.runtime}
            export PXR_PLUGINPATH_NAME=${p.pluginSet}
          '';
        };
      });
    };
}
