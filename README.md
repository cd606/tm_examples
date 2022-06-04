This is unstable software, with absolutely no warranty or guarantee whatsoever, whether express or implied, including but not limited to those of merchantability, fitness for specific purpose, or non-infringement.

This package provides some examples that use tm_infra, tm_basic and tm_transport. Some of these examples may require other third-party libraries.

The third-party software requirements (in addition to those required by tm_infra, tm_basic and tm_transport) by the example programs are usually listed under meson.build file in each program's source directory.

Windows note: linenoise-ng (https://github.com/arangodb/linenoise-ng), which is required under transaction_redundancy_test, poses a special problem for meson build because meson by default builds everything in "/MD" or "/MDd" mode, but linenoise-ng by default (either using the Visual Studio solution files provided under the source package, or through vcpkg) will use "/MT" or "/MTd" mode, and this will cause linking problems. The solution to this is to build linenoise-ng from source, but modify its CMakeLists.txt to use /MD or /MDd instead. This will cause the build to fail on its test executable, but at that point the library has already been successfully built. 

**NOTE: In recent versions of Visual Studio 2022, the build is broken, the compiler throws out impossible errors such as "'second' is not a member of std::pair" when encountering complex template code. It is found that by moving certain inner classes out of the containing class, the compiler may work for some of the code, but there are still codes that cannot be made to work this way. The error seems to be inside the compiler itself because in one test

  auto iter = someMap.find(...);
  auto const &v = iter->second;

works, but 

  auto iter = someMap.find(...);
  if (iter == someMap.end()) {return;}
  auto const &v = iter->second;

does not work (other codes are exactly the same). Therefore currently there is no estimate on when this might be fixed.