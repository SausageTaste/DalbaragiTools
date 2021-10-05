#include <vector>
#include <string>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <filesystem>

#include "daltools/model_parser.h"
#include "daltools/modifier.h"
#include "daltools/byte_tool.h"
#include "daltools/model_exporter.h"


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


    std::vector<uint8_t> read_file(const char* const path) {
        using namespace std::string_literals;

        std::ifstream file{ path, std::ios::ate | std::ios::binary | std::ios::in };

        if (!file.is_open())
            throw std::runtime_error("failed to open file: "s + path);

        const auto file_size = static_cast<size_t>(file.tellg());
        std::vector<uint8_t> buffer;
        buffer.resize(file_size);

        file.seekg(0);
        file.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
        file.close();

        return buffer;
    }

    dal::parser::Model load_model(const char* const path) {
        const auto model_data = ::read_file(path);
        const auto unzipped = dal::parser::unzip_dmd(model_data.data(), model_data.size());
        return dal::parser::parse_dmd(unzipped->data(), unzipped->size()).value();
    }

    void export_model(const char* const path, const dal::parser::Model& model) {
        const auto binary_built = dal::parser::build_binary_model(model, nullptr, nullptr);
        const auto zipped = dal::parser::zip_binary_model(binary_built->data(), binary_built->size());

        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char*>(zipped->data()), zipped->size());
        file.close();
    }

    void assert_or_runtime_error(const bool condition, const char* const msg) {
        if (!condition)
            throw std::runtime_error{ msg };
    }


    class ArgParser {

    private:
        std::string m_source_path, m_output_path;

        bool m_work_indexing = false;
        bool m_work_merge_by_material = false;
        bool m_work_flip_uv_coord_v = false;
        bool m_work_reduce_joints = false;

    public:
        ArgParser(int argc, char** argv) {
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

            for (int i = 1; i < argc; ++i) {
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


int main(int argc, char* argv[]) try {
    namespace dalp = dal::parser;

    const ::ArgParser parser{ argc, argv };
    ::Timer timer;

    std::cout << "Start for file '" << parser.source_path() << "'\n";

    std::cout << "    Model loading";
    timer.check();
    auto model = ::load_model(parser.source_path().c_str());
    std::cout << " done (" << timer.get_elapsed() << ")\n";

    if (parser.work_flip_uv_coord_v()) {
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

    if (parser.work_merge_by_material()) {
        std::cout << "    Merging by material";
        timer.check();

        model.m_units_straight = dal::parser::merge_by_material(model.m_units_straight);
        model.m_units_straight_joint = dal::parser::merge_by_material(model.m_units_straight_joint);
        model.m_units_indexed = dal::parser::merge_by_material(model.m_units_indexed);
        model.m_units_indexed_joint = dal::parser::merge_by_material(model.m_units_indexed_joint);

        std::cout << " done (" << timer.get_elapsed() << ")\n";
    }

    if (parser.work_indexing()) {
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

    if (parser.work_reduce_joints()) {
        std::cout << "    Reducing joints";
        timer.check();

        if (dal::parser::reduce_joints(model)) {
            std::cout << " done (" << timer.get_elapsed() << ")\n";
        }
        else {
            std::cout << " failed (" << timer.get_elapsed() << ")\n";
            return -1;
        }
    }

    std::cout << "    Exporting";
    timer.check();
    ::export_model(parser.output_path().c_str(), model);
    std::cout << " done to '" << parser.output_path() << "' (" << timer.get_elapsed() << ")\n";
    return 0;
}
catch (const std::runtime_error& e) {
    std::cout << "std::runtime_error: " << e.what() << std::endl;
}
