from urllib import request
import json

req = request.Request("http://localhost:34567/test_facility", json.dumps({
    'request': {
        'x': ['abc', 'def']
        , 'y': 2.0
    }
}).encode('utf-8'))
resp = request.urlopen(req)
print(resp.read())
resp = request.urlopen(req)
print(resp.read())
