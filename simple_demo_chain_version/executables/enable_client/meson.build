enable_client_gui_library = static_library(
    'enable_client_gui_library'
    , ['EnablerGUIDataFlow.cpp']
    , include_directories: inc
    , dependencies: [common_deps]
)
executable(
    'oneshot_enable_client'
    , ['OneShotMain.cpp']
    , include_directories: [inc]
    , link_with: [enable_client_gui_library]
    , dependencies: [common_deps]
)
executable(
    'linenoise_enable_client'
    , ['LineNoiseMain.cpp']
    , include_directories: [inc]
    , link_with: [enable_client_gui_library]
    , dependencies: [common_deps, linenoise_dep]
)

# GUI libraries are optional dependencies
ftxui_dep = dependency('ftxui', required: false)
if ftxui_dep.found()
    executable(
        'ftxui_enable_client'
        , ['FTXUIMain.cpp', prebuild_ftxui_info_header]
        , include_directories: [inc]
        , link_with: [enable_client_gui_library]
        , dependencies: [common_deps, ftxui_dep]
    )
    message('Building ftxui_enable_client (FTXUI found)')
else
    message('Skipping ftxui_enable_client (FTXUI not found)')
endif

fltk_dep = dependency('fltk', required: false)
if fltk_dep.found()
    executable(
        'fltk_enable_client'
        , ['FltkMain.cpp']
        , include_directories: [inc]
        , link_with: [enable_client_gui_library]
        , dependencies: [common_deps, fltk_dep]
        , gui_app: true
    )
    message('Building fltk_enable_client (FLTK found)')
else
    message('Skipping fltk_enable_client (FLTK not found)')
endif

if build_machine.system() == 'windows'
    wx = dependency('wxwidgets-vcpkg', required: false)
else
    wx = dependency('wxwidgets', required: false)
endif

if wx.found()
    executable(
        'wx_enable_client'
        , ['WxMain.cpp']
        , include_directories: [inc]
        , link_with: [enable_client_gui_library]
        , dependencies: [common_deps, wx]
        , gui_app: true
    )
    message('Building wx_enable_client (wxWidgets found)')
else
    message('Skipping wx_enable_client (wxWidgets not found)')
endif