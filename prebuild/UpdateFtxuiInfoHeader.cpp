#include <ftxui/component/component.hpp>

#include <type_traits>
#include <concepts>
#include <iostream>
#include <fstream>

template <typename T>
concept HasOnRender = requires(T t) {
    { t.OnRender() } -> std::same_as<ftxui::Element>;
};

int main(int, char **argv) {
    std::ofstream ofs(argv[1]);
    ofs << "#ifndef OMNIGLOT_TRADING_SYSTEM_PREBUILD_FTXUI_INFO_HPP_\n";
    ofs << "#define OMNIGLOT_TRADING_SYSTEM_PREBUILD_FTXUI_INFO_HPP_\n";
    ofs << "namespace com { namespace omniglot { namespace trading { namespace prebuild {\n";
    if constexpr (HasOnRender<ftxui::ComponentBase>) {
        ofs << "\tconstexpr bool FTXUI_COMPONENT_BASE_HAS_ON_RENDER = true;\n";
        ofs << "\t#define FTXUI_COMPONENT_BASE_HAS_ON_RENDER_FOR_MACRO 1\n";
    } else {
        ofs << "\tconstexpr bool FTXUI_COMPONENT_BASE_HAS_ON_RENDER = false;\n";
        ofs << "\t#define FTXUI_COMPONENT_BASE_HAS_ON_RENDER_FOR_MACRO 0\n";
    }
    ofs << "} } } }\n";
    ofs << "#endif\n";
    ofs.close();
    return 0;
}