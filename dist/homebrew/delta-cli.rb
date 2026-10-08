# typed: false
# frozen_string_literal: true

# Installs the prebuilt CLI from the public release repository. The source repository is private,
# so there is no build-from-source (--HEAD) path. The version and every sha256 below are written by
# scripts/update-package-manifests.cjs from the published release; do not edit them by hand.
class DeltaCli < Formula
  desc "Offline AI assistant powered by llama.cpp"
  homepage "https://github.com/nile-agi/delta-release"
  version "1.0.13"
  license "MIT"

  on_macos do
    on_arm do
      url "https://github.com/nile-agi/delta-release/releases/download/v1.0.13/delta-cli-macos-arm64.tar.gz"
      sha256 "b70d5521c3a4da44badcd4e1cc50298d39553795887e10a17a93db472603616e"
    end
    on_intel do
      url "https://github.com/nile-agi/delta-release/releases/download/v1.0.13/delta-cli-macos-x86_64.tar.gz"
      sha256 "91320893125efc86d696d10c098fc3e2b2eac5f0666fab93dc56fcba20ba2fbd"
    end
  end

  on_linux do
    on_intel do
      url "https://github.com/nile-agi/delta-release/releases/download/v1.0.13/delta-cli-linux-x86_64.tar.gz"
      sha256 "8185d2b0af3bc1cccd569656d82f6653ebed08931937ee2be10fbcc9d78a3da2"
    end
    depends_on "curl"
    depends_on "util-linux" # libuuid
  end

  def install
    # delta looks for delta-server and llama-server beside itself, and for the web UI in
    # share/delta-cli/webui.
    bin.install "bin/delta", "bin/delta-server", "bin/llama-server"
    (share/"delta-cli").install "webui"
  end

  test do
    assert_match version.to_s, shell_output("#{bin}/delta --version")
  end
end
