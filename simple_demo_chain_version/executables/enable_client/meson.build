enable_client_gui_library = static_library(
    'enable_client_gui_library'
    , ['EnablerGUIDataFlow.cpp']
    , include_directories: inc
    , dependencies: [common_deps]
)
executable(
    'ftxui_enable_client'
    , ['FTXUIMain.cpp']
    , include_directories: [inc]
    , link_with: [enable_client_gui_library]
    , dependencies: [common_deps, dependency('ftxui')]
)
executable(
    'fltk_enable_client'
    , ['FltkMain.cpp']
    , include_directories: [inc]
    , link_with: [enable_client_gui_library]
    , dependencies: [common_deps, dependency('fltk')]
    , gui_app: true
)
if build_machine.system() == 'windows'
    wx = dependency('wxwidgets-vcpkg')
else
    wx = dependency('wxwidgets')
endif
executable(
    'wx_enable_client'
    , ['WxMain.cpp']
    , include_directories: [inc]
    , link_with: [enable_client_gui_library]
    , dependencies: [common_deps, wx]
    , gui_app: true
)