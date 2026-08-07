fltk_dep = dependency('fltk', required: false)
if fltk_dep.found()
    executable(
        'fltk_client'
        , ['FltkClient.cpp', 'GuiClientDataFlow.cpp']
        , include_directories: inc
        , dependencies: [common_deps, fltk_dep]
        , gui_app: true
    )
    message('Building versionless fltk_client (FLTK found)')
else
    message('Skipping versionless fltk_client (FLTK not found)')
endif

if build_machine.system() == 'windows'
    wx = dependency('wxwidgets-vcpkg', required: false)
else
    wx = dependency('wxwidgets', required: false)
endif

if wx.found()
    executable(
        'wx_client'
        , ['WxClient.cpp', 'GuiClientDataFlow.cpp']
        , include_directories: inc
        , dependencies: [common_deps, wx]
        , gui_app: true
    )
    message('Building versionless wx_client (wxWidgets found)')
else
    message('Skipping versionless wx_client (wxWidgets not found)')
endif
