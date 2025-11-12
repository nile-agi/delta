int main() { 
    std::string exe_dir = tools::FileOps::get_executable_dir();
    std::cout << "Executable dir: [" << exe_dir << "]" << std::endl;
    std::string test_path = tools::FileOps::join_path(exe_dir, "build_macos/delta-server");
    std::cout << "Test path: [" << test_path << "]" << std::endl;
    std::cout << "File exists: " << (tools::FileOps::file_exists(test_path) ? "YES" : "NO") << std::endl;
    return 0;
}
