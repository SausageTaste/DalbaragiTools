#include <vector>
#include <string>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <filesystem>

#include <fmt/format.h>
#include <fmt/chrono.h>
#include <argparse/argparse.hpp>

#include "daltools/model_parser.h"
#include "daltools/modifier.h"
#include "daltools/byte_tool.h"
#include "daltools/model_exporter.h"
#include "daltools/konst.h"
#include "daltools/util.h"
#include "daltools/compression.h"


namespace {

    size_t calc_path_length(const std::filesystem::path& path) {
        size_t output = 0;

        for (auto& x : path)
            ++output;

        return output;
    }

    void create_folders_of_path(const std::filesystem::path& path, const size_t exclude_last_n) {
        namespace fs = std::filesystem;

        const auto path_length = ::calc_path_length(path);
        fs::path cur_path;

        size_t index = 0;
        for (const auto& x : path) {
            if ((path_length - (++index)) < exclude_last_n)
                break;

            cur_path /= x;

            if (!fs::is_directory(cur_path) && cur_path.u8string().size() > 3)
                fs::create_directory(cur_path);
        }
    }

    std::filesystem::path insert_suffix(std::filesystem::path path, const std::string& suffix) {
        const auto extension = path.extension();
        path.replace_extension("");
        path += suffix;
        path.replace_extension(extension);

        return path;
    }

    std::filesystem::path insert_subfolder_suffix(std::filesystem::path path, const std::string& subfolder, const std::string& suffix) {
        auto filename_ext = path.filename();
        const auto extension = path.extension();
        filename_ext.replace_extension("");
        filename_ext += suffix;
        filename_ext.replace_extension(extension);

        path.replace_filename("");
        path /= subfolder;
        path /= filename_ext;

        return path;
    }

    std::filesystem::path select_output_file_path(const std::string src_path, const std::optional<std::string> subfolder, const std::optional<std::string> suffix) {
        std::filesystem::path output_path;

        if (!suffix && !subfolder) {
            output_path = ::insert_suffix(src_path, "_");
        }
        else if (suffix && !subfolder) {
            output_path = ::insert_suffix(src_path, *suffix);
        }
        else if (!suffix && subfolder) {
            output_path = ::insert_subfolder_suffix(src_path, *subfolder, "");
            ::create_folders_of_path(output_path, 1);
        }
        else if (suffix && subfolder) {
            output_path = ::insert_subfolder_suffix(src_path, *subfolder, *suffix);
            ::create_folders_of_path(output_path, 1);
        }
        else {
            throw std::runtime_error{ "This shouldn't happen" };
        }

        return output_path;
    }


    template <typename T>
    std::optional<T> read_file(const char* const path) {
        using namespace std::string_literals;

        std::ifstream file{ path, std::ios::ate | std::ios::binary | std::ios::in };

        if (!file.is_open())
            return std::nullopt;

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
        return dal::parser::parse_dmd(model_data->data(), model_data->size()).value();
    }

    void export_model(
        const char* const path,
        const dal::parser::Model& model,
        const dal::crypto::PublicKeySignature::SecretKey* const sign_key,
        dal::crypto::PublicKeySignature* const sign_mgr
    ) {
        using namespace std::string_literals;

        const auto binary_built = dal::parser::build_binary_model(model, sign_key, sign_mgr);

        std::ofstream file(path, std::ios::binary);
        if (!file.is_open())
            throw std::runtime_error{ "Failed to open file to write: "s + path };

        file.write(reinterpret_cast<const char*>(binary_built->data()), binary_built->size());
        file.close();
    }

    void assert_or_runtime_error(const bool condition, const char* const msg) {
        if (!condition)
            throw std::runtime_error{ msg };
    }

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


// Operation main functions
namespace {

    void work_model_mod(int argc, char* argv[]) {
        namespace dalp = dal::parser;
        using namespace std::string_literals;

        dal::Timer timer;
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
        parser.add_argument("-p", "--print")
            .help("Print properties of model")
            .default_value(false)
            .implicit_value(true);

        parser.add_argument("-k", "--key")
            .help("Specify path to a text file that contains secret key generated by this app to sign it");
        parser.add_argument("-s", "--subfolder")
            .help("If set, output file goes into the subfolder");
        parser.add_argument("--suffix")
            .help("Set suffix for exported file name");

        parser.parse_args(argc, argv);

        const auto files = parser.get<std::vector<std::string>>("files");
        bool export_needed = false;

        std::cout << files.size() << " files provided" << std::endl;
        for (const auto& src_path : files) {
            std::cout << "\nStart for file '" << src_path << "'\n";

            std::cout << "    Model loading";
            timer.check();
            auto model = ::load_model(src_path.c_str());
            std::cout << " done (" << timer.get_elapsed() << ")\n";

            if (parser["--print"] == true) {
                std::cout << "    Print properties\n";
                std::cout << "        render units straight: " << model.m_units_straight.size() << '\n';
                std::cout << "        render units straight joint: " << model.m_units_straight_joint.size() << '\n';
                std::cout << "        render units indexed: " << model.m_units_indexed.size() << '\n';
                std::cout << "        render units indexed joint: " << model.m_units_indexed_joint.size() << '\n';
                std::cout << "        joints: " << model.m_skeleton.m_joints.size() << '\n';
                std::cout << "        animations: " << model.m_animations.size() << '\n';

                if (model.m_signature_hex.empty())
                    std::cout << "        signature: null\n";
                else
                    std::cout << "        signature: " << model.m_signature_hex << '\n';
            }

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

                export_needed = true;
                std::cout << " done (" << timer.get_elapsed() << ")\n";
            }

            if (parser["--merge"] == true) {
                std::cout << "    Merging by material";
                timer.check();

                model.m_units_straight = dal::parser::merge_by_material(model.m_units_straight);
                model.m_units_straight_joint = dal::parser::merge_by_material(model.m_units_straight_joint);
                model.m_units_indexed = dal::parser::merge_by_material(model.m_units_indexed);
                model.m_units_indexed_joint = dal::parser::merge_by_material(model.m_units_indexed_joint);

                export_needed = true;
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

                    export_needed = true;
                }
                model.m_units_straight.clear();

                for (const auto& unit : model.m_units_straight_joint) {
                    dalp::RenderUnit<dalp::Mesh_IndexedJoint> new_unit;
                    new_unit.m_name = unit.m_name;
                    new_unit.m_material = unit.m_material;
                    new_unit.m_mesh = dal::parser::convert_to_indexed(unit.m_mesh);
                    model.m_units_indexed_joint.push_back(new_unit);

                    export_needed = true;
                }
                model.m_units_straight_joint.clear();

                std::cout << " done (" << timer.get_elapsed() << ")\n";
            }

            if (parser["--reduce"] == true) {
                std::cout << "    Reducing joints";
                timer.check();

                const auto result = dal::parser::reduce_joints(model);
                switch (result) {
                    case dal::parser::JointReductionResult::success:
                        export_needed = true;
                        std::cout << " done (" << timer.get_elapsed() << ")\n";
                        break;
                    case dal::parser::JointReductionResult::needless:
                        std::cout << " skipped (" << timer.get_elapsed() << ")\n";
                        break;
                    default:
                        std::cout << " failed (" << timer.get_elapsed() << ")\n";
                        throw std::runtime_error{ "Failed reducing joints" };
                }
            }

            const auto key_path = parser.present("--key");
            if (key_path.has_value())
                export_needed = true;

            if (export_needed) {
                std::cout << "    Exporting";
                timer.check();

                const auto output_path = select_output_file_path(
                    src_path,
                    parser.present("--subfolder"),
                    parser.present("--suffix")
                );

                if (key_path.has_value()) {
                    const auto key_hex = ::read_file<std::string>(key_path->c_str());
                    const dal::crypto::PublicKeySignature::SecretKey sk{ *key_hex };
                    if (!sk.is_valid())
                        throw std::runtime_error{ "Input secret key is not valid" };

                    dal::crypto::PublicKeySignature sign_mgr{ dal::crypto::CONTEXT_PARSER };
                    ::export_model(output_path.u8string().c_str(), model, &sk, &sign_mgr);
                    std::cout << " and signing done to " << output_path << " (" << timer.get_elapsed() << ")\n";
                }
                else {
                    ::export_model(output_path.u8string().c_str(), model, nullptr, nullptr);
                    std::cout << " done to " << output_path << " (" << timer.get_elapsed() << ")\n";
                }
            }
        }
    }

    void work_key(int argc, char* argv[]) {
        dal::Timer timer;
        argparse::ArgumentParser parser{ "daltools" };
        {
        parser.add_argument("operation")
            .help("Operation name");

        parser.add_argument("-p", "--print")
            .help("Print attributes of a key")
            .default_value(false)
            .implicit_value(true);

        parser.add_argument("-i", "--input")
            .help("Key path")
            .required();
        }
        parser.parse_args(argc, argv);

        const auto key_path = parser.get<std::string>("--input");
        const auto [key, attrib] = dal::crypto::load_key<dal::crypto::IKey>(key_path.c_str());

        if (parser["--print"] == true) {
            fmt::print("Owner: {}\n", attrib.m_owner_name);
            fmt::print("E-mail: {}\n", attrib.m_email);
            fmt::print("Description: {}\n", attrib.m_description);
            fmt::print("Key type: {}\n", attrib.get_type_str());

            const auto a = std::chrono::system_clock::to_time_t(attrib.m_created_time);
            fmt::print("Created date: {:%F %T %z}\n", fmt::localtime(a));
            std::cout << "===================================\n";
        }
    }

    void work_keygen(int argc, char* argv[]) {
        dal::Timer timer;
        argparse::ArgumentParser parser{ "daltools" };
        {
        parser.add_argument("operation")
            .help("Operation name");

        parser.add_argument("-s", "--sign")
            .help("Generate key pair for signing")
            .default_value(false)
            .implicit_value(true);

        parser.add_argument("-o", "--output")
            .help("File path to save key files. Extension must be omitted")
            .required();

        parser.add_argument("--owner")
            .required();

        parser.add_argument("--email")
            .required();

        parser.add_argument("--description");
        }
        parser.parse_args(argc, argv);

        const auto output_prefix = parser.get<std::string>("--output");

        if (parser["--sign"] == true) {
            std::cout << "Keypair for signing\n";
            timer.check();

            dal::crypto::PublicKeySignature sign_mgr{ dal::crypto::CONTEXT_PARSER };
            const auto [pk, sk] = sign_mgr.gen_keys();

            dal::crypto::KeyAttrib attrib;
            attrib.m_owner_name = parser.get<std::string>("--owner");
            attrib.m_email = parser.get<std::string>("--email");
            attrib.m_description = parser.get<std::string>("--description");

            {
                attrib.m_type = sk.key_type();
                const auto path = output_prefix + "-sign_sec.dky";
                dal::crypto::save_key(path.c_str(), sk, attrib);
                fmt::print("    Secret key: {}\n", path);
            }

            {
                attrib.m_type = pk.key_type();
                const auto path = output_prefix + "-sign_pub.dky";
                dal::crypto::save_key(path.c_str(), pk, attrib);
                fmt::print("    Public key: {}\n", path);
            }

            fmt::print("    took {} seconds\n", timer.get_elapsed());
        }
    }

    void work_compile(int argc, char* argv[]) {
        argparse::ArgumentParser parser{ "daltools" };

        parser.add_argument("operation")
            .help("Operation name");

        parser.add_argument("-k", "--key")
            .help("Path to a secret key file");

        parser.add_argument("files")
            .help("Input model file paths")
            .remaining();

        parser.parse_args(argc, argv);

        const auto files = parser.get<std::vector<std::string>>("files");

        for (const auto& src_path : files) {
            const auto file_content = ::read_file<std::vector<uint8_t>>(src_path.c_str());
            std::vector<dal::parser::SceneIntermediate> scenes;
            const auto result = dal::parser::parse_json(scenes, file_content->data(), file_content->size());

            for (auto& scene : scenes) {
                dal::parser::flip_uv_vertically(scene);
                dal::parser::clear_collection_info(scene);
                dal::parser::optimize_scene(scene);
            }

            const auto model = dal::parser::convert_to_model_dmd(scenes.at(0));
            dal::parser::binary_buffer_t binary_built;
            const auto key_path = parser.present("--key");
            if (key_path.has_value()) {
                dal::crypto::PublicKeySignature sign_mgr{ dal::crypto::CONTEXT_PARSER };
                const auto [key, attrib] = dal::crypto::load_key<dal::crypto::PublicKeySignature::SecretKey>(key_path->c_str());
                binary_built = dal::parser::build_binary_model(model, &key, &sign_mgr).value();
            }
            else {
                binary_built = dal::parser::build_binary_model(model, nullptr, nullptr).value();
            }

            std::filesystem::path output_path = src_path;
            output_path.replace_extension("dmd");

            std::ofstream file(output_path.u8string().c_str(), std::ios::binary);
            file.write(reinterpret_cast<const char*>(binary_built.data()), binary_built.size());
            file.close();
        }
    }

    void work_verify(int argc, char* argv[]) {
        argparse::ArgumentParser parser{ "daltools" };

        parser.add_argument("operation")
            .help("Operation name");

        parser.add_argument("-k", "--key")
            .help("Path to a public key file");

        parser.add_argument("files")
            .help("Input model file paths")
            .remaining();

        parser.parse_args(argc, argv);

        const auto files = parser.get<std::vector<std::string>>("files");

        for (const auto& src_path : files) {
            const auto file_content = ::read_file<std::vector<uint8_t>>(src_path.c_str());

            const auto key_path = parser.get<std::string>("--key");
            const auto [key, attrib] = dal::crypto::load_key<dal::crypto::PublicKeySignature::PublicKey>(key_path.c_str());

            dal::parser::Model model;
            dal::crypto::PublicKeySignature sign_mgr{ dal::crypto::CONTEXT_PARSER };
            const auto result = parse_verify_dmd(model, file_content->data(), file_content->size(), key, sign_mgr);

            if (result != dal::parser::ModelParseResult::invalid_signature) {
                fmt::print("\"{}\" has a valid signature by\n", src_path);
                fmt::print("    Owner: {}\n", attrib.m_owner_name);
                fmt::print("    E-mail: {}\n", attrib.m_email);
            }
            else {
                fmt::print("\"{}\" has an invalid signature!!\n", src_path);
            }
        }
    }

}


int main(int argc, char* argv[]) try {
    using namespace std::string_literals;

    if (argc < 2)
        throw std::runtime_error{ "Operation must be specified" };

    if ("model"s == argv[1])
        ::work_model_mod(argc, argv);
    else if ("key"s == argv[1])
        ::work_key(argc, argv);
    else if ("keygen"s == argv[1])
        ::work_keygen(argc, argv);
    else if ("compile"s == argv[1])
        ::work_compile(argc, argv);
    else if ("verify"s == argv[1])
        ::work_verify(argc, argv);
    else
        throw std::runtime_error{ "unkown operation ("s + argv[1] + "). It must be one of { model, keygen }" };
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
