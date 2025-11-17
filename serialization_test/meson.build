executable(
    'serialization_test'
    , ['SerializationTest.cpp']
    , include_directories: inc
    , dependencies: [common_deps]
)
executable(
    'proto_interop_test'
    , ['ProtoInteropTest.cpp']
    , include_directories: inc
    , dependencies: [common_deps]
)
if build_machine.system() == 'windows'
  executable(
      'mongo_test'
      , ['MongoTest.cpp']
      , include_directories: inc
      , dependencies: [common_deps, dependency('libmongocxx-static'), dependency('libbsoncxx')]
  )
endif
executable(
    'variant_test'
    , ['VariantTest.cpp']
    , include_directories: inc
    , dependencies: [common_deps]
)
executable(
    'tuple_test'
    , ['TupleTest.cpp']
    , include_directories: inc
    , dependencies: [common_deps]
)
executable(
    'struct_test'
    , ['StructTest.cpp']
    , include_directories: inc
    , dependencies: [common_deps]
)
executable(
    'fixed_precision_decimal_test'
    , ['FixedPrecisionDecimalTest.cpp']
    , include_directories: inc
    , dependencies: [common_deps]
)
executable(
    'enum_test'
    , ['EnumTest.cpp']
    , include_directories: inc
    , dependencies: [common_deps]
)
