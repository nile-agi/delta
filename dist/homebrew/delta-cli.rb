# typed: false
# frozen_string_literal: true

class DeltaCli < Formula
  desc "Offline AI Assistant powered by llama.cpp"
  homepage "https://github.com/nile-agi/delta"
  license "MIT"

  # Stable release — pre-built binaries (no build tools required)
  # Uncomment and update when a GitHub release is published:
  # url "https://github.com/nile-agi/delta/releases/download/v#{version}/delta-cli-macos-#{Hardware::CPU.arm? ? "arm64" : "x86_64"}.tar.gz"
  # version "1.0.0"
  # sha256 "UPDATE_SHA256_FOR_RELEASE"

  head "https://github.com/nile-agi/delta.git", branch: "main", submodules: false

  depends_on "cmake" => :build
  depends_on "curl" => :build
  depends_on "pkg-config" => :build
  depends_on "node" => :build
  depends_on "pnpm" => :build

  on_macos do
    depends_on :macos
  end

  on_linux do
    depends_on "gcc" => :build
  end

  def install
    if build.head?
      install_from_source
    else
      install_from_binary
    end
  end

  def install_from_source
    ohai "Building Delta CLI from source..."

    system "git", "submodule", "update", "--init", "engine/vendor/llama.cpp"

    if Dir.exist?("engine/vendor/llama.cpp/.gitmodules")
      cd "engine/vendor/llama.cpp" do
        system "git", "submodule", "update", "--init", "--recursive" rescue nil
      end
    end

    # Build web UI
    if Dir.exist?("web/app")
      ohai "Building web UI..."
      cd "web/app" do
        system "pnpm", "install"
        system "pnpm", "run", "build"
      end
    end

    mkdir "build" do
      system "cmake", "..",
                    "-DCMAKE_BUILD_TYPE=Release",
                    "-DGGML_METAL=#{OS.mac? ? "ON" : "OFF"}",
                    "-DLLAMA_CUDA=OFF",
                    "-DLLAMA_VULKAN=OFF",
                    "-DLLAMA_HIPBLAS=OFF",
                    "-DBUILD_TESTS=OFF",
                    "-DUSE_CURL=ON",
                    *std_cmake_args
      system "make", "-j#{ENV.make_jobs}"
    end

    bin.install "build/delta"
    bin.install "build/delta-server"

    server_path = nil
    %w[
      build/bin/llama-server
      build/engine/vendor/llama.cpp/bin/llama-server
      build/engine/vendor/llama.cpp/tools/server/llama-server
      build/llama-server
      build/bin/server
      build/server
    ].each do |p|
      if File.exist?(p)
        server_path = p
        break
      end
    end

    if server_path
      bin.install server_path => "llama-server"
      ohai "Installed llama-server"
    else
      opoo "llama-server binary not found"
    end

    # Install Delta's custom web UI
    if Dir.exist?("public") && (File.exist?("public/index.html") || File.exist?("public/index.html.gz"))
      (share/"delta-cli").install "public" => "webui"
      ohai "Web UI installed"
    else
      opoo "Web UI not found in public/, skipping"
    end
  end

  def install_from_binary
    ohai "Installing pre-built Delta CLI..."

    bin.install "bin/delta"
    bin.install "bin/delta-server"
    bin.install "bin/llama-server" if File.exist?("bin/llama-server")

    if Dir.exist?("webui")
      (share/"delta-cli").install "webui"
    end
  end

  test do
    system "#{bin}/delta", "--version"
  end
end
