#ifndef VERIFYING_KEYS_HPP_
#define VERIFYING_KEYS_HPP_

#include <tm_kit/transport/security/SignatureHelper.hpp>

namespace simple_demo_chain_version {
    inline const dev::cd606::tm::transport::security::SignatureHelper::PublicKeyMap
    verifyingKeys = {
        {"calculator", { 
            0x9B,0x39,0xEE,0x8A,0xAB,0x24,0xD3,0x6B,0xFB,0xF2,0x79,0xC0,0x60,0x32,0x6D,0xF6,
            0x1E,0xA2,0x68,0x1D,0xDD,0x70,0x45,0x78,0x53,0xB7,0x12,0x38,0x10,0x13,0xED,0xAD 
        }}
        , {"main_logic", {
            0x97,0x17,0xC4,0x33,0xC6,0x24,0xA6,0xEF,0xD5,0x0C,0xB9,0xB5,0x02,0x1E,0xFF,0x38,
            0x54,0x57,0xCE,0x9A,0x45,0x3F,0x74,0x74,0x26,0x4B,0x3C,0x78,0x54,0xE3,0x07,0x50
        }}
        , {"place_request", {
            0xDE,0x1D,0xFB,0x0C,0x22,0x40,0x96,0xFD,0x90,0x3F,0x7D,0x36,0x10,0xA6,0xDA,0x75,
            0xE5,0x2A,0xE0,0x01,0xC3,0x5A,0x1D,0xE9,0x3D,0x72,0xF6,0xB8,0xA2,0x36,0xC1,0x9E
        }}
        , {"transcription", {
            0x66,0x32,0x6E,0x9F,0x1E,0xC0,0x25,0x62,0x21,0x02,0x7C,0xF6,0xD7,0xAB,0xB7,0x55,
            0x84,0x23,0xFE,0xCF,0xAA,0x02,0x21,0x94,0x55,0x08,0x65,0x13,0x5B,0x3B,0x57,0xD6
        }}
    };
}

#endif