import requests
import json

resp = requests.post(
    "https://localhost:34567/test_facility"
    , data=json.dumps({
        'request': {
            'x': ['abc', 'def']
            , 'y': 2.0
        }
    })
    , verify='../grpc_interop_test/DotNetServer/server.crt'
    , cert=('../grpc_interop_test/DotNetClient/client.crt','../grpc_interop_test/DotNetClient/client.key')
);
print(resp.text)