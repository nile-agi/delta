# Installs the prebuilt CLI from the public release repository (the source repository is private).
# The version and every sha256 below are written by scripts/update-package-manifests.cjs from the
# published release; do not edit them by hand.
{ lib
, stdenv
, fetchurl
, autoPatchelfHook
, curl
, libuuid
}:

let
  version = "1.0.13";
  sources = {
    aarch64-darwin = { asset = "delta-cli-macos-arm64.tar.gz"; sha256 = "b70d5521c3a4da44badcd4e1cc50298d39553795887e10a17a93db472603616e"; };
    x86_64-darwin = { asset = "delta-cli-macos-x86_64.tar.gz"; sha256 = "91320893125efc86d696d10c098fc3e2b2eac5f0666fab93dc56fcba20ba2fbd"; };
    x86_64-linux = { asset = "delta-cli-linux-x86_64.tar.gz"; sha256 = "8185d2b0af3bc1cccd569656d82f6653ebed08931937ee2be10fbcc9d78a3da2"; };
  };
  source = sources.${stdenv.hostPlatform.system}
    or (throw "delta-cli has no prebuilt release for ${stdenv.hostPlatform.system}");
in
stdenv.mkDerivation {
  pname = "delta-cli";
  inherit version;

  src = fetchurl {
    url = "https://github.com/nile-agi/delta-release/releases/download/v${version}/${source.asset}";
    inherit (source) sha256;
  };

  # The Linux binaries link the system libcurl and libuuid; point them at the Nix store.
  nativeBuildInputs = lib.optionals stdenv.isLinux [ autoPatchelfHook ];
  buildInputs = lib.optionals stdenv.isLinux [ curl libuuid stdenv.cc.cc.lib ];

  dontConfigure = true;
  dontBuild = true;

  installPhase = ''
    runHook preInstall

    # delta looks for delta-server and llama-server beside itself, and for the web UI in
    # share/delta-cli/webui.
    install -Dm755 -t $out/bin bin/delta bin/delta-server bin/llama-server
    mkdir -p $out/share/delta-cli
    cp -r webui $out/share/delta-cli/webui

    runHook postInstall
  '';

  meta = with lib; {
    description = "Offline AI assistant powered by llama.cpp";
    longDescription = ''
      Delta runs large language models directly on your device via llama.cpp, with a web UI,
      model management and GPU acceleration.
    '';
    homepage = "https://github.com/nile-agi/delta-release";
    license = licenses.mit;
    sourceProvenance = [ sourceTypes.binaryNativeCode ];
    maintainers = [ ];
    platforms = builtins.attrNames sources;
    mainProgram = "delta";
  };
}
