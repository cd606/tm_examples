QT       += core gui charts

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    LineSeries.cpp \
    main.cpp \
    mainwindow.cpp \
    tmPart.cpp

HEADERS += \
    LineSeries.hpp \
    SignalObject.hpp \
    mainwindow.h \
    tmPart.hpp

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

win32:CONFIG(release, debug|release): QMAKE_LFLAGS += /NODEFAULTLIB:LIBCMT

VCPKGDIR = ..\..\..\..\..\..\..\vcpkg\installed\x64-windows
#VCPKGDIR = c:\Users\diyu6\vcpkg\installed\x64-windows

INCLUDEPATH += c:\include $${VCPKGDIR}\include
LIBS += c:\lib\libtm_kit_infra.a c:\lib\libtm_kit_basic.a c:\lib\libtm_kit_transport.a
LIBS += $${VCPKGDIR}\lib\spdlog.lib $${VCPKGDIR}\lib\fmt.lib
LIBS += $${VCPKGDIR}\lib\crossguid.lib
LIBS += C:\lib\librabbitmq.4.lib c:\lib\rabbitmq.4.lib
LIBS += $${VCPKGDIR}\..\x64-windows-static\lib\hiredis.lib
LIBS += $${VCPKGDIR}\lib\libzmq-mt-4_3_4.lib
LIBS += $${VCPKGDIR}\lib\nng.lib
LIBS += bcrypt.lib
LIBS += $${VCPKGDIR}\lib\libssl.lib $${VCPKGDIR}\lib\libcrypto.lib
LIBS += $${VCPKGDIR}\lib\libsodium.lib

LIBS += $${VCPKGDIR}\lib\grpc++.lib $${VCPKGDIR}\lib\grpc.lib
LIBS += $${VCPKGDIR}\lib\address_sorting.lib $${VCPKGDIR}\lib\re2.lib $${VCPKGDIR}\lib\upb.lib $${VCPKGDIR}\lib\upb_fastdecode.lib $${VCPKGDIR}\lib\upb_textformat.lib $${VCPKGDIR}\lib\upb_reflection.lib $${VCPKGDIR}\lib\utf8_range.lib $${VCPKGDIR}\lib\grpc_upbdefs.lib $${VCPKGDIR}\lib\cares.lib
LIBS += $${VCPKGDIR}\lib\gpr.lib $${VCPKGDIR}\lib\zlib.lib $${VCPKGDIR}\lib\zstd.lib
LIBS += $${VCPKGDIR}\lib\abseil_dll.lib $${VCPKGDIR}\lib\absl_flags.lib $${VCPKGDIR}\lib\absl_flags_commandlineflag.lib $${VCPKGDIR}\lib\absl_flags_commandlineflag_internal.lib $${VCPKGDIR}\lib\absl_flags_config.lib $${VCPKGDIR}\lib\absl_flags_internal.lib $${VCPKGDIR}\lib\absl_flags_marshalling.lib $${VCPKGDIR}\lib\absl_flags_parse.lib $${VCPKGDIR}\lib\absl_flags_private_handle_accessor.lib $${VCPKGDIR}\lib\absl_flags_program_name.lib $${VCPKGDIR}\lib\absl_flags_reflection.lib $${VCPKGDIR}\lib\absl_flags_usage.lib $${VCPKGDIR}\lib\absl_flags_usage_internal.lib $${VCPKGDIR}\lib\absl_random_internal_distribution_test_util.lib $${VCPKGDIR}\lib\absl_statusor.lib $${VCPKGDIR}\lib\absl_strerror.lib $${VCPKGDIR}\lib\absl_wyhash.lib
LIBS += $${VCPKGDIR}\lib\libprotobuf.lib
