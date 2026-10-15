import c,json
N='NiagaraToolsets.NiagaraToolset_System'
A='editor_toolset.toolsets.asset.AssetTools'
ROOT='/Game/Detonauto/Effects'
def sysref(n): return {"refPath":f"{ROOT}/{n}.{n}"}
T={'vec2':'/Script/CoreUObject.Vector2f','v3f':'/Script/CoreUObject.Vector3f','float':'/Script/Niagara.NiagaraFloat','int':'/Script/Niagara.NiagaraInt32','vec3':'/Script/CoreUObject.Vector','pos':'/Script/Niagara.NiagaraPosition','color':'/Script/CoreUObject.LinearColor'}
def dv(t,v):
    if t in('float','int'): val={"value":v}
    elif t in('vec3','pos','v3f'): val=dict(zip('xyz',v))
    elif t=='vec2': val=dict(zip('xy',v))
    elif t=='color': val=dict(zip('rgba',v))
    return {"struct":{"refPath":T[t]},"value":val}
def uservars(n,spec):
    vs=[{"name":k,"description":"","type":{"classStructOrEnum":{"refPath":T[t]}},"defaultValue":dv(t,v)} for k,(t,v) in spec.items()]
    return c.call(N,'AddUserVariables',{"system":sysref(n),"variablesToAdd":vs})
def ref(n,em,script,module,inp):
    return {"system":sysref(n),"emitterName":em,"scriptName":script,"moduleName":module,"rendererIndex":-1,"inputNameStack":inp}
def link(n,em,script,module,inp,var,t):
    return c.call(N,'SetStackInputData',{"stackInputRef":ref(n,em,script,module,[inp]),"inputData":{"struct":{"refPath":"/Script/NiagaraEditor.NiagaraExt_StackInputData_Linked"},"value":{"linkedVariable":{"name":var,"type":{"classStructOrEnum":{"refPath":T[t]}}}}}})
def setv(n,em,script,module,inp,t,v):
    return c.call(N,'SetStackInputData',{"stackInputRef":ref(n,em,script,module,[inp]),"inputData":dv(t,v)})
def hlsl(n,em,script,module,inp,expr):
    return c.call(N,'SetStackInputData',{"stackInputRef":ref(n,em,script,module,[inp]),"inputData":{"struct":{"refPath":"/Script/NiagaraEditor.NiagaraExt_StackInputData_HlslExpression"},"value":{"hlslExpression":expr}}})
