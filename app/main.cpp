#include <vector>
#include <string>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <filesystem>

#include <argparse/argparse.hpp>

#include "daltools/model_parser.h"
#include "daltools/modifier.h"
#include "daltools/byte_tool.h"
#include "daltools/model_exporter.h"
#include "daltools/konst.h"


namespace {

    constexpr unsigned MISCROSEC_PER_SEC = 1000000;
    constexpr unsigned NANOSEC_PER_SEC = 1000000000;

    class Timer {

    private:
        std::chrono::steady_clock::time_point m_last_checked = std::chrono::steady_clock::now();

    public:
        void check() {
            this->m_last_checked = std::chrono::steady_clock::now();
        }

        double get_elapsed() const {
            const auto deltaTime_microsec = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - this->m_last_checked).count();
            return static_cast<double>(deltaTime_microsec) / static_cast<double>(MISCROSEC_PER_SEC);
        }

        double check_get_elapsed() {
            const auto now = std::chrono::steady_clock::now();
            const auto deltaTime_microsec = std::chrono::duration_cast<std::chrono::microseconds>(now - this->m_last_checked).count();
            this->m_last_checked = now;

            return static_cast<double>(deltaTime_microsec) / static_cast<double>(MISCROSEC_PER_SEC);
        }

    protected:
        auto& last_checked(void) const {
            return this->m_last_checked;
        }

    };


    template <typename T>
    T read_file(const char* const path) {
        using namespace std::string_literals;

        std::ifstream file{ path, std::ios::ate | std::ios::binary | std::ios::in };

        if (!file.is_open())
            throw std::runtime_error("failed to open file: "s + path);

        const auto file_size = static_cast<size_t>(file.tellg());
        T buffer;
        buffer.resize(file_size);

        file.seekg(0);
        file.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
        file.close();

        return buffer;
    }

    dal::parser::Model load_model(const char* const path) {
        const auto model_data = ::read_file<std::vector<uint8_t>>(path);
        const auto unzipped = dal::parser::unzip_dmd(model_data.data(), model_data.size());
        return dal::parser::parse_dmd(unzipped->data(), unzipped->size()).value();
    }

    void export_model(
        const char* const path,
        const dal::parser::Model& model,
        const dal::crypto::PublicKeySignature::SecretKey* const sign_key,
        dal::crypto::PublicKeySignature* const sign_mgr
    ) {
        using namespace std::string_literals;

        const auto binary_built = dal::parser::build_binary_model(model, sign_key, sign_mgr);
        const auto zipped = dal::parser::zip_binary_model(binary_built->data(), binary_built->size());

        std::ofstream file(path, std::ios::binary);
        if (!file.is_open())
            throw std::runtime_error{ "Failed to open file to write: "s + path };

        file.write(reinterpret_cast<const char*>(zipped->data()), zipped->size());
        file.close();
    }

    void assert_or_runtime_error(const bool condition, const char* const msg) {
        if (!condition)
            throw std::runtime_error{ msg };
    }

    std::string insert_suffix(const std::string& path_str, const std::string& suffix) {
        std::filesystem::path path{ path_str };
        const auto extension = path.extension();
        path.replace_extension("");
        path += suffix;
        path.replace_extension(extension);
        return path.u8string();
    }

    std::string insert_subfolder_suffix(const std::string& path_str, const std::string& subfolder, const std::string& suffix) {
        std::filesystem::path path{ path_str };

        auto filename_ext = path.filename();
        const auto extension = path.extension();
        filename_ext.replace_extension("");
        filename_ext += suffix;
        filename_ext.replace_extension(extension);

        path.replace_filename("");
        path /= subfolder;
        path /= filename_ext;

        return path.u8string();
    }


    enum class KeyType{ sign, unknown };


    class ArgParser_Model {

    private:
        std::string m_source_path, m_output_path;
        std::string m_secret_key_path;

        bool m_work_indexing = false;
        bool m_work_merge_by_material = false;
        bool m_work_flip_uv_coord_v = false;
        bool m_work_reduce_joints = false;

    public:
        ArgParser_Model(int argc, char** argv) {
            this->parse(argc, argv);
        }

        void assert_integrity() const {
            namespace fs = std::filesystem;
            using namespace std::string_literals;

            if (this->m_source_path.empty())
                throw std::runtime_error{ "source path has not been provided" };
            if (this->m_output_path.empty())
                throw std::runtime_error{ "output path has not been provided" };

            if (!fs::exists(this->m_source_path))
                throw std::runtime_error{ "source file doesn't exist: "s + this->m_source_path };
//          if (fs::exists(this->m_output_path))
//              throw std::runtime_error{ "output file already exists: "s + this->m_output_path };
        }

        auto& source_path() const {
            return this->m_source_path;
        }

        auto& output_path() const {
            return this->m_output_path;
        }

        auto& secret_key_path() const {
            return this->m_secret_key_path;
        }

        bool work_indexing() const {
            return this->m_work_indexing;
        }

        bool work_merge_by_material() const {
            return this->m_work_merge_by_material;
        }

        bool work_flip_uv_coord_v() const {
            return this->m_work_flip_uv_coord_v;
        }

        bool work_reduce_joints() const {
            return this->m_work_reduce_joints;
        }

    private:
        void parse(const int argc, const char *const *const argv) {
            using namespace std::string_literals;

            for (int i = 2; i < argc; ++i) {
                const auto one = argv[i];

                if ('-' == one[0]) {
                    switch (one[1]) {
                        case 's':
                            ::assert_or_runtime_error(++i < argc, "source path(-s) needs a parameter");
                            this->m_source_path = argv[i];
                            break;
                        case 'o':
                            ::assert_or_runtime_error(++i < argc, "output path(-o) needs a parameter");
                            this->m_output_path = argv[i];
                            break;
                        case 'k':
                            ::assert_or_runtime_error(++i < argc, "secret key path(-k) needs a parameter");
                            this->m_secret_key_path = argv[i];
                            break;
                        case 'i':
                            this->m_work_indexing = true;
                            break;
                        case 'm':
                            this->m_work_merge_by_material = true;
                            break;
                        case 'v':
                            this->m_work_flip_uv_coord_v = true;
                            break;
                        case 'r':
                            this->m_work_reduce_joints = true;
                            break;
                        default:
                            throw std::runtime_error{ "unkown argument: "s + one };
                    }
                }
                else {
                    throw std::runtime_error{ "unkown argument: "s + one };
                }
            }

            this->assert_integrity();
        }

    };


    class ArgParser_Keygen {

    private:
        std::string m_output_path;
        KeyType m_key_type = KeyType::unknown;

    public:
        ArgParser_Keygen(int argc, char** argv) {
            this->parse(argc, argv);
        }

        void assert_integrity() const {
            namespace fs = std::filesystem;

            if (this->m_output_path.empty())
                throw std::runtime_error{ "output path has not been provided" };
        }

        auto key_type() const {
            return this->m_key_type;
        }

        auto& output_path() const {
            return this->m_output_path;
        }

    private:
        void parse(const int argc, const char *const *const argv) {
            using namespace std::string_literals;

            if ("sign"s == argv[2])
                this->m_key_type = KeyType::sign;

            if (KeyType::unknown == this->m_key_type)
                throw std::runtime_error{ "unkown key type: "s + argv[2] };

            for (int i = 3; i < argc; ++i) {
                const auto one = argv[i];

                if ('-' == one[0]) {
                    switch (one[1]) {
                        case 'o':
                            ::assert_or_runtime_error(++i < argc, "output path(-o) needs a parameter");
                            this->m_output_path = argv[i];
                            break;
                        default:
                            throw std::runtime_error{ "unkown argument: "s + one };
                    }
                }
                else {
                    throw std::runtime_error{ "unkown argument: "s + one };
                }
            }

            this->assert_integrity();
        }

    };


    class ArgParser_ModelPrint {

    private:
        std::string m_source_path;

    public:
        ArgParser_ModelPrint(int argc, char** argv) {
            this->parse(argc, argv);
        }

        void assert_integrity() const {
            namespace fs = std::filesystem;
            using namespace std::string_literals;

            if (this->m_source_path.empty())
                throw std::runtime_error{ "source path has not been provided" };

            if (!fs::exists(this->m_source_path))
                throw std::runtime_error{ "source file doesn't exist: "s + this->m_source_path };
        }

        auto& source_path() const {
            return this->m_source_path;
        }

    private:
        void parse(const int argc, const char *const *const argv) {
            using namespace std::string_literals;

            for (int i = 2; i < argc; ++i) {
                const auto one = argv[i];

                if ('-' == one[0]) {
                    switch (one[1]) {
                        case 's':
                            ::assert_or_runtime_error(++i < argc, "source path(-s) needs a parameter");
                            this->m_source_path = argv[i];
                            break;
                        default:
                            throw std::runtime_error{ "unkown argument: "s + one };
                    }
                }
                else {
                    throw std::runtime_error{ "unkown argument: "s + one };
                }
            }

            this->assert_integrity();
        }

    };

}


namespace {

    void flip_uv_vertically(dal::parser::Mesh_Straight& mesh) {
        const auto vert_size = mesh.m_vertices.size() / 3;

        for (size_t i = 0; i < vert_size; ++i) {
            mesh.m_texcoords[2 * i + 1] = 1.f - mesh.m_texcoords[2 * i + 1];
        }
    }

    void flip_uv_vertically(dal::parser::Mesh_StraightJoint& mesh) {
        const auto vert_size = mesh.m_vertices.size() / 3;

        for (size_t i = 0; i < vert_size; ++i) {
            mesh.m_texcoords[2 * i + 1] = 1.f - mesh.m_texcoords[2 * i + 1];
        }
    }

    void flip_uv_vertically(dal::parser::Mesh_Indexed& mesh) {
        for (auto& vert : mesh.m_vertices) {
            vert.m_uv_coords.y = 1.f - vert.m_uv_coords.y;
        }
    }

    void flip_uv_vertically(dal::parser::Mesh_IndexedJoint& mesh) {
        for (auto& vert : mesh.m_vertices) {
            vert.m_uv_coords.y = 1.f - vert.m_uv_coords.y;
        }
    }

}


namespace {

    void work_model_mod(int argc, char* argv[]) {
        namespace dalp = dal::parser;
        using namespace std::string_literals;

        ::Timer timer;
        argparse::ArgumentParser parser{ "daltools" };

        parser.add_argument("operation")
            .help("Operation name");
        parser.add_argument("files")
            .help("Input model file paths")
            .remaining();

        parser.add_argument("-i", "--index")
            .help("Convert all vertices into indexed structure")
            .default_value(false)
            .implicit_value(true);
        parser.add_argument("-m", "--merge")
            .help("Merge vertices by material")
            .default_value(false)
            .implicit_value(true);
        parser.add_argument("-f", "--flip")
            .help("Flip uv coordinates' v components")
            .default_value(false)
            .implicit_value(true);
        parser.add_argument("-r", "--reduce")
            .help("Remove joints which are not used in bundled animations")
            .default_value(false)
            .implicit_value(true);

        parser.add_argument("-k", "--key")
            .help("Specify path to a text file that contains secret key generated by this app to sign it");
        parser.add_argument("-s", "--suffix")
            .help("Set suffix for exported file name")
            .default_value<std::string>("_");
        parser.add_argument("--subfolder")
            .help("If set, output file goes into the subfolder");

        parser.parse_args(argc, argv);

        auto files = parser.get<std::vector<std::string>>("files");
        std::cout << files.size() << " files provided" << std::endl;
        for (const auto& src_path : files) {
            std::cout << "Start for file '" << src_path << "'\n";

            std::cout << "    Model loading";
            timer.check();
            auto model = ::load_model(src_path.c_str());
            std::cout << " done (" << timer.get_elapsed() << ")\n";

            if (parser["--flip"] == true) {
                std::cout << "    Flipping uv coords vertically";
                timer.check();

                for (auto& unit : model.m_units_straight) {
                    ::flip_uv_vertically(unit.m_mesh);
                }
                for (auto& unit : model.m_units_straight_joint) {
                    ::flip_uv_vertically(unit.m_mesh);
                }
                for (auto& unit : model.m_units_indexed) {
                    ::flip_uv_vertically(unit.m_mesh);
                }
                for (auto& unit : model.m_units_indexed_joint) {
                    ::flip_uv_vertically(unit.m_mesh);
                }

                std::cout << " done (" << timer.get_elapsed() << ")\n";
            }

            if (parser["--merge"] == true) {
                std::cout << "    Merging by material";
                timer.check();

                model.m_units_straight = dal::parser::merge_by_material(model.m_units_straight);
                model.m_units_straight_joint = dal::parser::merge_by_material(model.m_units_straight_joint);
                model.m_units_indexed = dal::parser::merge_by_material(model.m_units_indexed);
                model.m_units_indexed_joint = dal::parser::merge_by_material(model.m_units_indexed_joint);

                std::cout << " done (" << timer.get_elapsed() << ")\n";
            }

            if (parser["--index"] == true) {
                std::cout << "    Indexing";
                timer.check();

                for (const auto& unit : model.m_units_straight) {
                    dalp::RenderUnit<dalp::Mesh_Indexed> new_unit;
                    new_unit.m_name = unit.m_name;
                    new_unit.m_material = unit.m_material;
                    new_unit.m_mesh = dal::parser::convert_to_indexed(unit.m_mesh);
                    model.m_units_indexed.push_back(new_unit);
                }
                model.m_units_straight.clear();

                for (const auto& unit : model.m_units_straight_joint) {
                    dalp::RenderUnit<dalp::Mesh_IndexedJoint> new_unit;
                    new_unit.m_name = unit.m_name;
                    new_unit.m_material = unit.m_material;
                    new_unit.m_mesh = dal::parser::convert_to_indexed(unit.m_mesh);
                    model.m_units_indexed_joint.push_back(new_unit);
                }
                model.m_units_straight_joint.clear();

                std::cout << " done (" << timer.get_elapsed() << ")\n";
            }

            if (parser["--reduce"] == true) {
                std::cout << "    Reducing joints";
                timer.check();

                if (dal::parser::reduce_joints(model)) {
                    std::cout << " done (" << timer.get_elapsed() << ")\n";
                }
                else {
                    std::cout << " failed (" << timer.get_elapsed() << ")\n";
                    throw std::runtime_error{ "Failed reducing joints" };
                }
            }

            std::cout << "    Exporting";
            timer.check();

            std::string output_path;
            if (const auto subfolder = parser.present("--subfolder")) {
                output_path = ::insert_subfolder_suffix(src_path, subfolder.value(), parser.get<std::string>("--suffix"));
            }
            else {
                output_path = ::insert_suffix(src_path, parser.get<std::string>("--suffix"));
            }

            if (auto key_path = parser.present("--key")) {
                dal::crypto::PublicKeySignature sign_mgr{ dal::crypto::CONTEXT_PARSER };
                const auto key_hex = ::read_file<std::string>(key_path->c_str());
                const dal::crypto::PublicKeySignature::SecretKey sk{ key_hex };
                ::export_model(output_path.c_str(), model, &sk, &sign_mgr);
            }
            else {
                ::export_model(output_path.c_str(), model, nullptr, nullptr);
            }

            std::cout << " done to '" << output_path << "' (" << timer.get_elapsed() << ")\n";
        }
    }

    void work_keygen(int argc, char* argv[]) {
        ::ArgParser_Keygen parser{ argc, argv };

        std::cout << parser.output_path() << std::endl;

        switch (parser.key_type()) {
            case ::KeyType::sign:
            {
                dal::crypto::PublicKeySignature sign_mgr{ dal::crypto::CONTEXT_PARSER };
                const auto [pk, sk] = sign_mgr.gen_keys();

                {
                    std::ofstream file(parser.output_path() + "-secret.dky", std::ios::binary);
                    const auto data = sk.make_hex_str();
                    file.write(data.data(), data.size());
                    file.close();
                }

                {
                    std::ofstream file(parser.output_path() + "-public.dky", std::ios::binary);
                    const auto data = pk.make_hex_str();
                    file.write(data.data(), data.size());
                    file.close();
                }
            }
        }
    }

    void work_model_print(int argc, char* argv[]) {
        ::Timer timer;

        ArgParser_ModelPrint parser{ argc, argv };

        std::cout << "    Model loading";
        timer.check();
        const auto model = ::load_model(parser.source_path().c_str());
        std::cout << " done (" << timer.get_elapsed() << ")\n";

        std::cout << "        render units straight:       " << model.m_units_straight.size() << std::endl;
        std::cout << "        render units straight joint: " << model.m_units_straight_joint.size() << std::endl;
        std::cout << "        render units indexed:        " << model.m_units_indexed.size() << std::endl;
        std::cout << "        render units indexed joint:  " << model.m_units_indexed_joint.size() << std::endl;
        std::cout << "        joints: " << model.m_skeleton.m_joints.size() << std::endl;
        std::cout << "        animations: " << model.m_animations.size() << std::endl;
        std::cout << "        signature: " << model.m_signature_hex << std::endl;
    }

}


int main(int argc, char* argv[]) try {
    using namespace std::string_literals;

    if (argc < 2)
        throw std::runtime_error{ "Operation must be specified" };

    if ("model"s == argv[1])
        ::work_model_mod(argc, argv);
    else if ("keygen"s == argv[1])
        ::work_keygen(argc, argv);
    else if ("modelprint"s == argv[1])
        ::work_model_print(argc, argv);
    else
        throw std::runtime_error{ "unkown operation: "s + argv[1] };
}
catch (const std::runtime_error& e) {
    std::cout << "\nstd::runtime_error: " << e.what() << std::endl;
}
catch (const std::exception& e) {
    std::cout << "\nstd::exception: " << e.what() << std::endl;
}
catch (...) {
    std::cout << "\nunknown object thrown: " << std::endl;
}
