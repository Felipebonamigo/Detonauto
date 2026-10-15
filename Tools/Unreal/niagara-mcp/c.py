import json,sys,urllib.request
U='http://127.0.0.1:8010/mcp'
H={'Content-Type':'application/json','Accept':'application/json, text/event-stream'}
sid=open('sid').read().strip()
def rpc(method,params,i=3):
    r=urllib.request.Request(U,json.dumps({"jsonrpc":"2.0","id":i,"method":method,"params":params}).encode(),{**H,'Mcp-Session-Id':sid})
    return json.loads(urllib.request.urlopen(r,timeout=300).read().decode('utf8'))
def call(toolset,tool,args=None):
    d=rpc('tools/call',{"name":"call_tool","arguments":{"toolset_name":toolset,"tool_name":tool,"arguments":args or {}}})
    if 'error' in d: return d['error']
    res=d['result']; txt=res['content'][0]['text'] if res.get('content') else ''
    return ('ERR ' if res.get('isError') else '')+txt
if __name__=='__main__':
    ts,tool=sys.argv[1],sys.argv[2]; args=json.loads(sys.argv[3]) if len(sys.argv)>3 else {}
    print(call(ts,tool,args))
