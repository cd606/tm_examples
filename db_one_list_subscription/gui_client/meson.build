executable(
    'fltk_client'
    , ['FltkClient.cpp', 'GuiClientDataFlow.cpp']
    , include_directories: inc
    , dependencies: [common_deps, dependency('fltk')]
    , gui_app: true
)

if build_machine.system() == 'windows'
    wx = dependency('wxwidgets-vcpkg')
else
    wx = dependency('wxwidgets')
endif

executable(
    'wx_client'
    , ['WxClient.cpp', 'GuiClientDataFlow.cpp']
    , include_directories: inc
    , dependencies: [common_deps, wx]
    , gui_app: true
)