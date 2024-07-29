#pragma once


namespace dal {

    void work_compile(int argc, char* argv[]);
    void work_key(int argc, char* argv[]);
    void work_keygen(int argc, char* argv[]);
    void work_bundle(int argc, char* argv[]);
    void work_bundle_view(int argc, char* argv[]);


    using work_func_t = decltype(&work_compile);

}
