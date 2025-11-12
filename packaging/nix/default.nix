{ lib
, stdenv
, fetchFromGitHub
, cmake
, curl
, pkg-config
, makeWrapper
, darwin
, buildPackages
}:

stdenv.mkDerivation rec {
  pname = "delta-cli";
  version = "1.0.0";

  src = fetchFromGitHub {
    owner = "nile-agi";
    repo = "delta";
    rev = "v${version}";
    sha256 = "PLACEHOLDER_SHA256";
    fetchSubmodules = true;  # Always fetch latest llama.cpp submodule
  };

  nativeBuildInputs = [
    cmake
    curl
    pkg-config
    makeWrapper
  ] ++ lib.optionals stdenv.isDarwin [
    darwin.apple_sdk.frameworks.Metal
    darwin.apple_sdk.frameworks.Foundation
    darwin.apple_sdk.frameworks.Accelerate
  ];

  buildInputs = [
    curl
  ];

  cmakeFlags = [
    "-DCMAKE_BUILD_TYPE=Release"
    "-DGGML_METAL=${if stdenv.isDarwin then "ON" else "OFF"}"
    "-DLLAMA_CUDA=OFF"
    "-DLLAMA_VULKAN=OFF"
    "-DLLAMA_HIPBLAS=OFF"
    "-DBUILD_SERVER=ON"
    "-DUSE_CURL=ON"
  ] ++ lib.optionals stdenv.isDarwin [
    "-DCMAKE_C_COMPILER=${buildPackages.clang}/bin/clang"
    "-DCMAKE_CXX_COMPILER=${buildPackages.clang}/bin/clang++"
  ];

  buildPhase = ''
    runHook preBuild
    
    mkdir -p build
    cd build
    cmake .. $cmakeFlags
    make -j$NIX_BUILD_CORES
    
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    
    install -Dm755 build/delta $out/bin/delta
    install -Dm755 build/delta-server $out/bin/delta-server
    
    if [ -d vendor/llama.cpp/tools/server/public ]; then
      mkdir -p $out/share/delta-cli
      cp -r vendor/llama.cpp/tools/server/public $out/share/delta-cli/webui
    fi
    
    runHook postInstall
  '';

  meta = with lib; {
    description = "Offline AI Assistant powered by llama.cpp";
    longDescription = ''
      Delta CLI is an open-source, offline-first AI assistant that runs
      large language models (LLMs) directly on your device. Built on top
      of llama.cpp, Delta CLI provides a simple command-line interface
      to interact with AI models without requiring internet connectivity
      or cloud services.
    '';
    homepage = "https://github.com/nile-agi/delta";
    license = licenses.mit;
    maintainers = [ ];
    platforms = platforms.unix;
    mainProgram = "delta";
  };
}

