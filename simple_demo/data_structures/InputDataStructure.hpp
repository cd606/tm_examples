#ifndef DATA_STRUCTURES_INPUT_DATA_STRUCTURE_HPP_
#define DATA_STRUCTURES_INPUT_DATA_STRUCTURE_HPP_

#include <tm_kit/basic/SerializationHelperMacros.hpp>

namespace simple_demo {
    #define INPUT_DATA_FIELDS \
        ((double, value))
    
    TM_BASIC_CBOR_CAPABLE_STRUCT(InputDataPOCO, INPUT_DATA_FIELDS);
}

TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE(simple_demo::InputDataPOCO, INPUT_DATA_FIELDS);

#undef INPUT_DATA_FIELDS

#endif