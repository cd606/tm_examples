import requests
import json

resp = requests.get(
    "https://localhost:34567/test.html?"
    , verify='../grpc_interop_test/DotNetServer/server.crt'
    , cert=('../grpc_interop_test/DotNetClient/client.crt','../grpc_interop_test/DotNetClient/client.key')
    , auth=('user2', 'abcde')
)
print(resp.text)

resp = requests.post(
    "https://localhost:34567/test_facility"
    , data=json.dumps({
        'request': {
            'x': ['abc', 'def']
            , 'y': 2.0
           #, 't' : [1, 0.2]
            , 'tChoice': {'index': 1, 'content': 1}
        }
    })
    , verify='../grpc_interop_test/DotNetServer/server.crt'
    , cert=('../grpc_interop_test/DotNetClient/client.crt','../grpc_interop_test/DotNetClient/client.key')
    , auth=('user2', 'abcde')
);
print(resp.text)

resp = requests.get(
    "https://localhost:34567/test_facility_2?iParam=2&sParam=abc+de%28x%29#ab"
    , verify='../grpc_interop_test/DotNetServer/server.crt'
    , cert=('../grpc_interop_test/DotNetClient/client.crt','../grpc_interop_test/DotNetClient/client.key')
    , auth=('user2', 'abcde')
);
print(resp.text)
