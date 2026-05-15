"""
DMED — Dynamic MCP Endpoint Discovery Protocol — Python Library
Zero deps for core. Optional: zeroconf (mDNS), flask (server), requests (client)
"""
__version__ = "0.2.0"
__all__ = ["Beacon","Card","Tool","Transport","ServiceType","Flag","DMEDServer","DMEDScanner","DMEDClient","crc16","crc32","instance_id_from"]
import struct,json
from dataclasses import dataclass,field
from typing import Optional,List,Dict,Any
from enum import IntEnum,IntFlag

class Flag(IntFlag):
    AUTH=1;TLS=2;MULTI=4;CLOUD=8

class ServiceType(IntEnum):
    UNKNOWN=0;IOT_DEVICE=1;MEDIA=2;APPLIANCE=3;VEHICLE=4;RETAIL_KIOSK=5
    INFRASTRUCTURE=6;COMPUTING=7;AI_SERVICE=8;DATA_SOURCE=9;TOOL_UTILITY=10
    COMMUNICATION=11;HEALTH_MEDICAL=12;INDUSTRIAL=13;ENVIRONMENTAL=14
    SECURITY=15;INFORMATION=16;ENERGY=17;CUSTOM=255

def crc16(data:bytes)->int:
    c=0xFFFF
    for b in data:
        c^=b<<8
        for _ in range(8):c=((c<<1)^0x1021 if c&0x8000 else c<<1)&0xFFFF
    return c

def crc32(data:bytes)->int:
    c=0xFFFFFFFF
    for b in data:
        c^=b
        for _ in range(8):c=(c>>1)^(0xEDB88320&-(c&1))
    return(~c)&0xFFFFFFFF

def instance_id_from(s:str)->str:return f"{crc32(s.encode()):08x}"

@dataclass
class Beacon:
    version:int=1;flags:int=0;service_type:int=0;instance_id:int=0
    tx_power:Optional[int]=None;name_hash:Optional[int]=None

    def encode(self)->bytes:
        buf=struct.pack(">BBL",((self.version&0xF)<<4)|(self.flags&0xF),self.service_type&0xFF,self.instance_id&0xFFFFFFFF)
        if self.tx_power is not None:buf+=struct.pack("b",self.tx_power)
        if self.name_hash is not None:buf+=struct.pack(">H",self.name_hash&0xFFFF)
        return buf

    @classmethod
    def decode(cls,data:bytes)->"Beacon":
        if len(data)<6:raise ValueError("Too short")
        b0,st,iid=struct.unpack(">BBL",data[:6])
        b=cls(version=(b0>>4)&0xF,flags=b0&0xF,service_type=st,instance_id=iid)
        if len(data)>=7:b.tx_power=struct.unpack("b",data[6:7])[0]
        if len(data)>=9:b.name_hash=struct.unpack(">H",data[7:9])[0]
        return b

    @property
    def instance_id_hex(self)->str:return f"{self.instance_id:08x}"

    def to_mdns_txt(self,name="",path="/mcp",card="/dmed/card")->Dict[str,str]:
        d={"v":str(self.version),"id":self.instance_id_hex,"st":f"{self.service_type:02x}","fl":f"{self.flags:x}","path":path,"card":card}
        if name:d["nm"]=name
        return d

@dataclass
class Tool:
    name:str;description:str="";input_schema:Optional[Dict]=None

@dataclass
class Transport:
    type:str;url:str="";priority:int=10

@dataclass
class Card:
    instance_id:str;name:str;service_type:str="unknown";description:str=""
    transports:List[Transport]=field(default_factory=list)
    auth_type:str="none";tools:List[Tool]=field(default_factory=list)
    tags:List[str]=field(default_factory=list);metadata:Dict[str,Any]=field(default_factory=dict)

    def to_dict(self)->dict:
        d={"dmed_version":__version__,"instance_id":self.instance_id,"name":self.name,"service_type":self.service_type,
           "transports":[{"type":t.type,"url":t.url,"priority":t.priority}for t in self.transports],
           "auth":{"type":self.auth_type},"capabilities":{"tools":[{"name":t.name,"description":t.description,"inputSchema":t.input_schema or{"type":"object","properties":{}}}for t in self.tools],"resources":[],"prompts":[]}}
        if self.description:d["description"]=self.description
        if self.tags:d["tags"]=self.tags
        if self.metadata:d["metadata"]=self.metadata
        return d

    def to_json(self,indent=None)->str:return json.dumps(self.to_dict(),indent=indent)

    @classmethod
    def from_dict(cls,d:dict)->"Card":
        return cls(instance_id=d["instance_id"],name=d["name"],service_type=d.get("service_type","unknown"),
                   description=d.get("description",""),
                   transports=[Transport(t["type"],t.get("url",""),t.get("priority",10))for t in d.get("transports",[])],
                   auth_type=d.get("auth",{}).get("type","none"),
                   tools=[Tool(t["name"],t.get("description",""),t.get("inputSchema"))for t in d.get("capabilities",{}).get("tools",[])],
                   tags=d.get("tags",[]),metadata=d.get("metadata",{}))

    @classmethod
    def from_json(cls,s:str)->"Card":return cls.from_dict(json.loads(s))

class DMEDServer:
    """Full DMED server with discovery + interaction. Requires: pip install zeroconf flask"""
    def __init__(self,card:Card,port:int=8080,tool_handler=None):
        self.card=card;self.port=port;self._h=tool_handler or(lambda n,a:f"No handler for {n}")

    def start(self):
        import socket
        from zeroconf import Zeroconf,ServiceInfo
        from flask import Flask,jsonify,request as req
        ip=self._ip()
        if self.card.transports:self.card.transports[0].url=f"http://{ip}:{self.port}/mcp"
        beacon=Beacon(flags=Flag.MULTI if len(self.card.tools)>1 else 0,
                      service_type=getattr(ServiceType,self.card.service_type.upper(),0),
                      instance_id=int(self.card.instance_id,16))
        zc=Zeroconf()
        zc.register_service(ServiceInfo("_mcp-dmed._tcp.local.",f"{self.card.name}._mcp-dmed._tcp.local.",
            addresses=[socket.inet_aton(ip)],port=self.port,
            properties=beacon.to_mdns_txt(self.card.name),server=f"{self.card.instance_id}.local."))
        app=Flask(__name__)
        @app.route("/dmed/card")
        def _c():return jsonify(self.card.to_dict())
        @app.route("/mcp",methods=["POST"])
        def _m():
            r=req.get_json();m=r.get("method");rid=r.get("id")
            if m=="initialize":return jsonify({"jsonrpc":"2.0","id":rid,"result":{"protocolVersion":"2025-03-26","capabilities":{"tools":{}},"serverInfo":{"name":self.card.name,"version":__version__}}})
            if m=="notifications/initialized":return"",204
            if m=="tools/list":return jsonify({"jsonrpc":"2.0","id":rid,"result":{"tools":[{"name":t.name,"description":t.description,"inputSchema":t.input_schema or{"type":"object","properties":{}}}for t in self.card.tools]}})
            if m=="tools/call":
                try:res=self._h(r["params"]["name"],r["params"].get("arguments",{}));return jsonify({"jsonrpc":"2.0","id":rid,"result":{"content":[{"type":"text","text":str(res)}]}})
                except Exception as e:return jsonify({"jsonrpc":"2.0","id":rid,"error":{"code":-32603,"message":str(e)}})
            return jsonify({"jsonrpc":"2.0","id":rid,"error":{"code":-32601,"message":f"Unknown: {m}"}})
        # v0.2: Lightweight action endpoint
        @app.route("/dmed/action",methods=["POST"])
        def _action():
            r=req.get_json()
            action=r.get("action");params=r.get("params",{})
            if not action:return jsonify({"status":"error","message":"Missing 'action' field"}),400
            tool_names=[t.name for t in self.card.tools]
            if action not in tool_names:return jsonify({"status":"error","message":f"Unknown action: {action}. Available: {tool_names}"}),404
            try:
                res=self._h(action,params)
                return jsonify({"status":"ok","action":action,"result":{"text":str(res)}})
            except Exception as e:return jsonify({"status":"error","action":action,"message":str(e)}),500
        @app.route("/dmed/actions")
        def _actions():return jsonify({"actions":[{"name":t.name,"description":t.description,"params":t.input_schema or{"type":"object","properties":{}}}for t in self.card.tools]})
        print(f"[DMED] ✓ {self.card.name} at {ip}:{self.port}")
        print(f"[DMED]   Card:    http://{ip}:{self.port}/dmed/card")
        print(f"[DMED]   Actions: http://{ip}:{self.port}/dmed/actions")
        print(f"[DMED]   Invoke:  POST http://{ip}:{self.port}/dmed/action")
        print(f"[DMED]   MCP:     POST http://{ip}:{self.port}/mcp")
        try:app.run(host="0.0.0.0",port=self.port)
        finally:zc.unregister_all_services();zc.close()

    @staticmethod
    def _ip():
        import socket;s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
        try:s.connect(("8.8.8.8",80));return s.getsockname()[0]
        finally:s.close()

class DMEDClient:
    """v0.2: Interact with a discovered DMED endpoint. Requires: pip install requests"""
    def __init__(self,base_url:str,auth_token:str=None):
        self.base_url=base_url.rstrip("/")
        self._headers={"Content-Type":"application/json"}
        if auth_token:self._headers["Authorization"]=f"Bearer {auth_token}"
        self._card:Optional[Card]=None

    def connect(self)->Card:
        """Fetch the endpoint's card (capabilities)."""
        import requests
        r=requests.get(f"{self.base_url}/dmed/card",headers=self._headers,timeout=5)
        r.raise_for_status()
        self._card=Card.from_dict(r.json())
        return self._card

    def list_actions(self)->List[Dict]:
        """List available actions on the endpoint."""
        import requests
        r=requests.get(f"{self.base_url}/dmed/actions",headers=self._headers,timeout=5)
        r.raise_for_status()
        return r.json()["actions"]

    def send_action(self,action:str,params:Dict[str,Any]=None)->Dict:
        """Send a lightweight action/command to the endpoint."""
        import requests
        payload={"action":action,"params":params or{}}
        r=requests.post(f"{self.base_url}/dmed/action",json=payload,headers=self._headers,timeout=10)
        r.raise_for_status()
        return r.json()

    def call_tool(self,tool_name:str,arguments:Dict[str,Any]=None)->Dict:
        """Full MCP tools/call (JSON-RPC). Use send_action() for lightweight interaction."""
        import requests
        payload={"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":tool_name,"arguments":arguments or{}}}
        r=requests.post(f"{self.base_url}/mcp",json=payload,headers=self._headers,timeout=10)
        r.raise_for_status()
        return r.json().get("result",{})

    @property
    def card(self)->Optional[Card]:return self._card

class DMEDScanner:
    """Scan for DMED endpoints. Requires: pip install zeroconf requests"""
    def __init__(self):self.endpoints:Dict[str,Card]={}
    def scan(self,timeout:float=5.0)->Dict[str,Card]:
        import socket,time,requests
        from zeroconf import Zeroconf,ServiceBrowser,ServiceListener
        sc=self
        class L(ServiceListener):
            def add_service(self,zc,st,name):
                info=zc.get_service_info(st,name)
                if not info:return
                h=socket.inet_ntoa(info.addresses[0]);p=info.port
                props={k.decode():v.decode()for k,v in info.properties.items()}
                try:card=Card.from_dict(requests.get(f"http://{h}:{p}{props.get('card','/dmed/card')}",timeout=3).json());sc.endpoints[card.instance_id]=card
                except:pass
            def remove_service(self,*a):pass
            def update_service(self,*a):pass
        zc=Zeroconf();ServiceBrowser(zc,"_mcp-dmed._tcp.local.",L())
        time.sleep(timeout);zc.close()
        return self.endpoints

    def connect(self,instance_id:str)->Optional["DMEDClient"]:
        """Connect to a discovered endpoint by instance_id. Returns a DMEDClient."""
        card=self.endpoints.get(instance_id)
        if not card or not card.transports:return None
        return DMEDClient(card.transports[0].url)
