#ifndef RPC_INTERFACE_HPP_
#define RPC_INTERFACE_HPP_

#include <tm_kit/basic/SerializationHelperMacros.hpp>

namespace rpc_examples {
    #define INPUT_FIELDS \
        ((int, x)) \
        ((std::string, y))
    #define OUTPUT_FIELDS \
        ((std::string, result))

    //We have four RPC methods, all using the same input-output types as above
    
    //The simple method takes one input, and returns one output (result = y+":"+to_string(x))
    
    //The client stream method takes multiple inputs, and returns one output (all the y+":"+to_string(x) values, separated by comma)
    //, the server understands that the number of inputs that the client will send is the 
    //value of x in the first input
    
    //The server stream method takes one input, and returns multiple output (y+":"+to_string(x), repeated x times)
    
    //The client stream-server stream takes multiple inputs and returns multiple output (for each input, it gives one y+":"+to_string(x))
    //, the server will only send these outputs after the client has finished sending its inputs
    //, and the server understands that the number of inputs that the client will send is the 
    //value of x in the first input

    //Since all the methods use the same data types, we only need to define the data types once
    TM_BASIC_CBOR_CAPABLE_STRUCT(Input, INPUT_FIELDS);
    TM_BASIC_CBOR_CAPABLE_STRUCT(Output, OUTPUT_FIELDS);
}

TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(rpc_examples::Input, INPUT_FIELDS);
TM_BASIC_CBOR_CAPABLE_STRUCT_SERIALIZE_NO_FIELD_NAMES(rpc_examples::Output, OUTPUT_FIELDS);

#endif