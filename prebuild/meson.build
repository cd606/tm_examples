prebuild_update_ftxui_info_header_exe = executable(
  'prebuild_update_ftxui_info_header'
  , ['UpdateFtxuiInfoHeader.cpp']
  , include_directories: inc
  , dependencies: common_deps
)
prebuild_ftxui_info_header = custom_target(
  'FtxuiInfo.hpp'
  , input: []
  , output: ['FtxuiInfo.hpp']
  , depends : prebuild_update_ftxui_info_header_exe
  , command: [
    prebuild_update_ftxui_info_header_exe
    , '@OUTPUT@'
  ]
  , install: false
  , install_dir: get_option('includedir')+'/prebuild'
)
