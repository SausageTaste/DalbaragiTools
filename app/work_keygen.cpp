#include "work_functions.hpp"

#include <fmt/format.h>
#include <argparse/argparse.hpp>

#include "daltools/common/crypto.h"
#include "daltools/common/konst.h"
#include "daltools/common/util.h"


namespace dal {

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

        parser.add_argument("-p", "--prefix")
            .help("File path to save key files. Extension must be omitted")
            .required();

        parser.add_argument("--owner")
            .required();

        parser.add_argument("--email")
            .required();

        parser.add_argument("--description")
            .default_value(std::string{});
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

}
