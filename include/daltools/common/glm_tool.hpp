#pragma once

#include <glm/glm.hpp>
#include <sung/general/linalg.hpp>


namespace dal {

    template <typename T>
    glm::tvec2<T>& vec_cast(sung::TVec2<T>& v) {
        static_assert(sizeof(glm::tvec2<T>) == sizeof(T) * 2);
        static_assert(sizeof(glm::tvec2<t>) == sizeof(sung::TVec2<T>));
        return *reinterpret_cast<glm::tvec2<T>*>(&v);
    }

    template <typename T>
    const glm::tvec2<T>& vec_cast(const sung::TVec2<T>& v) {
        return *reinterpret_cast<const glm::tvec2<T>*>(&v);
    }

    template <typename T>
    sung::TVec2<T>& vec_cast(glm::tvec2<T>& v) {
        return *reinterpret_cast<sung::TVec2<T>*>(&v);
    }

    template <typename T>
    const sung::TVec2<T>& vec_cast(const glm::tvec2<T>& v) {
        return *reinterpret_cast<const sung::TVec2<T>*>(&v);
    }

}  // namespace dal
