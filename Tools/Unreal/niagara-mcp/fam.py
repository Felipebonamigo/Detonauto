import c,ns,json,time
N=ns.N;A=ns.A;E='DirectionalBurst'
SRC='/Game/Detonauto/Effects/NS_DNSparkImpact'
def dup(name):
    print('dup',name,c.call(A,'duplicate',{"path":SRC,"new_path":"/Game/Detonauto/Effects/"+name}))
def inp(n,sc,mod,i,t,v,em=E): return c.call(N,'SetStackInputData',{"stackInputRef":ns.ref(n,em,sc,mod,i if isinstance(i,list) else [i]),"inputData":ns.dv(t,v)})[:70]
def enum(n,sc,mod,i,path,name,em=E):
    return c.call(N,'SetStackInputData',{"stackInputRef":ns.ref(n,em,sc,mod,[i]),"inputData":{"struct":{"refPath":"/Script/NiagaraEditor.NiagaraExt_StackInputData_Enum"},"value":{"enum":{"refPath":path},"enumName":name}}})[:90]
def hl(n,sc,mod,i,expr,em=E):
    return c.call(N,'SetStackInputData',{"stackInputRef":ns.ref(n,em,sc,mod,[i] if isinstance(i,str) else i),"inputData":{"struct":{"refPath":"/Script/NiagaraEditor.NiagaraExt_StackInputData_HlslExpression"},"value":{"hlslExpression":expr}}})[:90]
def rend(n,props,idx=0,em=E):
    r={"system":ns.sysref(n),"emitterName":em,"scriptName":"","moduleName":"","rendererIndex":idx,"inputNameStack":[]}
    return c.call(N,'SetRendererData',{"renderer":r,"rendererData":{"propertyValues":json.dumps(props)}})[:60]
def status(n):
    time.sleep(4)
    r=json.loads(c.call(N,'GetSystemCompileState',{"system":ns.sysref(n)}))['returnValue']
    return r['aggregateStatus'],r['bHasErrors'],r['bIsCompiling']
def save(n): return c.call(A,'save_assets',{"asset_paths":["/Game/Detonauto/Effects/"+n]})
P='ParticleSpawnScript';U='ParticleUpdateScript'
