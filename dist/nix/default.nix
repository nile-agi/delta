{ lib
, stdenv
, fetchFromGitHub
, cmake
, curl
, pkg-config
, makeWrapper
, nodejs
, nodePackages
, darwin
}:

stdenv.mkDerivation rec {
  pname = "delta-cli";
  version = "1.0.0";

  src = fetchFromGitHub {
    owner = "nile-agi";
    repo = "delta";
    rev = "v${version}";
    sha256 = "PLACEHOLDER_SHA256";  # Update when release is published
    fetchSubmodules = true;
  };

  nativeBuildInputs = [
    cmake
    curl
    pkg-config
    makeWrapper
    nodejs
    nodePackages.pnpm
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
    "-DBUILD_TESTS=OFF"
    "-DUSE_CURL=ON"
    "-DFORCE_SYSTEM_CLANG=OFF"
  ];

  preBuild = ''
    # Build web UI before C++ compilation
    cd $src/web/app
    export HOME=$TMPDIR
    pnpm install --frozen-lockfile
    pnpm run build
    cd $NIX_BUILD_TOP/$sourceRoot
  '';

  installPhase = ''
    runHook preInstall

    install -Dm755 delta $out/bin/delta
    install -Dm755 delta-server $out/bin/delta-server

    # Install llama-server
    if [ -f bin/llama-server ]; then
      install -Dm755 bin/llama-server $out/bin/llama-server
    elif [ -f llama-server ]; then
      install -Dm755 llama-server $out/bin/llama-server
    fi

    # Install Delta's custom web UI
    if [ -f $NIX_BUILD_TOP/$sourceRoot/public/index.html ]; then
      mkdir -p $out/share/delta-cli
      cp -r $NIX_BUILD_TOP/$sourceRoot/public $out/share/delta-cli/webui
    fi

    runHook postInstall
  '';

  meta = with lib; {
    description = "Offline AI Assistant powered by llama.cpp";
    longDescription = ''
      Delta CLI is an offline-first AI assistant that runs large language
      models directly on your device via llama.cpp. Includes a web UI,
      model management, and GPU acceleration (Metal, CUDA, Vulkan).
    '';
    homepage = "https://github.com/nile-agi/delta";
    license = licenses.mit;
    maintainers = [ ];
    platforms = platforms.unix;
    mainProgram = "delta";
  };
}
