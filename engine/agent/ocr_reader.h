#ifndef DELTA_AGENT_OCR_READER_H
#define DELTA_AGENT_OCR_READER_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <vector>

namespace delta {
namespace agent {

// A small OCR model run by its own llama-server, to read scanned PDF pages.
struct OcrModel {
    std::string id;
    std::string display_name;
    std::string repo;
    std::string model_file;
    std::string mmproj_file;
    long long size_bytes = 0; // model + mmproj
    int min_ram_gb = 0;
    std::string prompt;
    std::vector<std::string> server_args;
};

const std::vector<OcrModel>& ocr_models();

// An installed model first (never download twice), else the largest one this machine fits.
const OcrModel& pick_ocr_model(int ram_gb, const std::function<bool(const OcrModel&)>& installed);

// The page text without layout tags, code fences or surrounding blank lines.
std::string clean_ocr_text(const std::string& raw);

// /v1/chat/completions body for one page image (a data: URL).
nlohmann::json ocr_request(const OcrModel& model, const std::string& image_data_url);

std::vector<std::string> ocr_server_args(const OcrModel& model, const std::string& dir, int port);

// The llama-server binary shipped next to Delta (never delta-server), or "" when none.
std::string find_llama_server(const std::string& exe_dir);

class OcrReader {
  public:
    struct Hooks {
        // Downloads url to dest, reporting (bytes done, bytes total); false on failure or cancel.
        std::function<bool(const std::string& url, const std::string& dest,
                           const std::function<void(long long, long long)>& progress)>
            download;
        std::function<void()> cancel;
    };

    struct Page {
        int status = 200; // 409 not installed, 503 reader unavailable
        std::string text;
        std::string error;
    };

    static OcrReader& instance();
    ~OcrReader();

    void configure(const std::string& root_dir, const std::string& server_bin, int ram_gb, Hooks hooks);
    const OcrModel& model() const;
    bool installed() const;
    nlohmann::json status() const;
    // False when it is already installed or downloading.
    bool start_download();
    void cancel_download();
    Page read_page(const std::string& image_data_url);
    void stop();

    // Tests: send pages to a server already on this port instead of starting one.
    void use_server_for_tests(int port);
    void set_idle_seconds(int seconds);

  private:
    std::string dir() const;
    bool ensure_server(std::string& error);
    void stop_locked();
    void watch_idle();

    mutable std::mutex mutex_;
    std::mutex read_mutex_;
    std::string root_;
    std::string server_bin_;
    int ram_gb_ = 0;
    Hooks hooks_;
    const OcrModel* model_ = nullptr;

    std::atomic<bool> downloading_{false};
    std::atomic<bool> cancelled_{false};
    std::atomic<long long> done_bytes_{0};
    std::string download_error_;

    long long pid_ = 0;       // 0 when no server runs
    void* process_ = nullptr; // Windows process handle
    int port_ = 0;
    bool test_server_ = false;
    int idle_seconds_ = 90;
    std::chrono::steady_clock::time_point last_used_;
    std::thread idle_thread_;
    std::condition_variable idle_cv_;
    bool quit_ = false;
};

} // namespace agent
} // namespace delta

#endif // DELTA_AGENT_OCR_READER_H
