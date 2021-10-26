import requests
import json

class BearerAuth(requests.auth.AuthBase):
    def __init__(self, token):
        self.token = token
    def __call__(self, r):
        r.headers["authorization"] = "Bearer " + self.token
        return r

resp = requests.post(
    "https://localhost:56790/__API_AUTHENTICATION"
    , data=json.dumps({
        'username': 'user2'
        , 'password' : 'abcde'
    })
    , verify='../../grpc_interop_test/DotNetServer/server.crt'
    , cert=('../../grpc_interop_test/DotNetClient/client.crt','../../grpc_interop_test/DotNetClient/client.key')
)
token = resp.text
resp = requests.post(
    "https://localhost:56790/key_query"
    , data = "{}"
    , verify='../../grpc_interop_test/DotNetServer/server.crt'
    , cert=('../../grpc_interop_test/DotNetClient/client.crt','../../grpc_interop_test/DotNetClient/client.key')
    , auth=BearerAuth(token)
)
print(resp.text)