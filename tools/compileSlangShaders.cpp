#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <cstdlib>
#include <algorithm>

namespace fs = std::filesystem;

int main() {
    fs::path script_path = fs::current_path(); // Assuming executable runs in script dir
    fs::path shaders_source_dir = script_path / "../shaders/source";
    fs::path shaders_out_dir = script_path / "../shaders/out";
    std::string slang_compiler = "slangc";

    fs::create_directories(shaders_out_dir);

    // Delete existing .spv files
    for (auto& p : fs::directory_iterator(shaders_out_dir)) {
        if (p.path().extension() == ".spv") {
            fs::remove(p.path());
            std::cout << "Deleted: " << p.path() << "\n";
        }
        if (p.path().extension() == ".slang-module") {
            fs::remove(p.path());
            std::cout << "Deleted: " << p.path() << "\n";
        }
    }

    std::cout << "Searching in " << shaders_source_dir << "\n";

    for (auto& p : fs::directory_iterator(shaders_source_dir)) {
        if (p.path().extension() == ".slang") {
            std::string shader_file_full = p.path().string();
            std::string shader_file_name = p.path().filename().string();

            std::string shader_name, shader_type;
            size_t first_dot = shader_file_name.find('.');
            size_t last_dot = shader_file_name.rfind('.');
            if (first_dot != last_dot) {
                shader_name = shader_file_name.substr(0, first_dot);
                shader_type = shader_file_name.substr(first_dot + 1, last_dot - first_dot - 1);
            } else {
                shader_name = shader_file_name.substr(0, last_dot);
                shader_type = "mod";
            }

            std::string shader_type_long;
            if (shader_type == "vert") shader_type_long = "vertex";
            else if (shader_type == "frag") shader_type_long = "fragment";
            else if (shader_type == "comp") shader_type_long = "compute";

            std::string cmd;
            fs::path output_file;

            if (shader_type == "mod") {
                output_file = shaders_out_dir / (shader_name + ".slang-module");
                cmd = slang_compiler + " " + shader_file_full + " -o " + output_file.string() +
                      " -I " + shaders_source_dir.string() +
                      " -module-name " + shader_name;
                std::system(cmd.c_str());
                std::cout << shader_name << ".slang-module\n";
            } else {
                output_file = shaders_out_dir / (shader_name + "." + shader_type + ".spv");
                cmd = slang_compiler + " " + shader_file_full + " -o " + output_file.string() +
                      " -I " + shaders_source_dir.string() +
                      " -stage " + shader_type_long +
                      " -profile glsl_460 -target spirv -O3";
                std::system(cmd.c_str());
                std::cout << shader_name << "." << shader_type << ".spv\n";
            }
        }
    }

    return 0;
}