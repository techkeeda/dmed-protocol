# DMED Cookbook

Copy-paste recipes for making anything discoverable via DMED.

---

## 1. Temperature Sensor

```python
from dmed import Card, Tool, Transport, DMEDServer
import random

card = Card("temp0001", "Room Sensor", "environmental",
    transports=[Transport("http")],
    tools=[Tool("get_temperature", "Current temperature in Celsius")])

DMEDServer(card, 8080, lambda n, a: f"{20+random.random()*5:.1f}°C").start()
```

## 2. File Sharing

```python
from dmed import Card, Tool, Transport, DMEDServer

card = Card("file0001", "Shared Notes", "information",
    transports=[Transport("http")],
    tools=[Tool("read", "Read file content")])

DMEDServer(card, 8080, lambda n, a: open("/tmp/notes.txt").read()).start()
```

## 3. Local LLM

```python
from dmed import Card, Tool, Transport, DMEDServer

card = Card("llm00001", "Local Llama", "ai_service",
    transports=[Transport("http")],
    tools=[Tool("generate", "Generate text from prompt"),
           Tool("summarize", "Summarize text")])

def handle(name, args):
    # Replace with actual model call
    if name == "generate": return f"[Response to: {args.get('prompt','')}]"
    if name == "summarize": return f"[Summary of {len(args.get('text',''))} chars]"

DMEDServer(card, 8080, handle).start()
```

## 4. Smart Lock

```python
from dmed import Card, Tool, Transport, DMEDServer

card = Card("lock0001", "Front Door", "security",
    transports=[Transport("http")], auth_type="pin",
    tools=[Tool("lock", "Lock the door"), Tool("unlock", "Unlock the door"),
           Tool("status", "Get lock status")])

state = {"locked": True}
def handle(name, args):
    if name == "lock": state["locked"] = True; return "Locked"
    if name == "unlock": state["locked"] = False; return "Unlocked"
    if name == "status": return f"{'Locked' if state['locked'] else 'Unlocked'}"

DMEDServer(card, 8080, handle).start()
```

## 5. Any CLI Tool as MCP Endpoint

```python
import subprocess
from dmed import Card, Tool, Transport, DMEDServer

card = Card("cli00001", "System Info", "computing",
    transports=[Transport("http")],
    tools=[Tool("uptime", "System uptime"), Tool("disk", "Disk usage"),
           Tool("memory", "Memory usage")])

def handle(name, args):
    cmds = {"uptime": "uptime", "disk": "df -h /", "memory": "free -h"}
    return subprocess.check_output(cmds[name], shell=True).decode().strip()

DMEDServer(card, 8080, handle).start()
```

## Pattern: Wrap Anything in 4 Lines

```python
from dmed import Card, Tool, Transport, DMEDServer

card = Card("<id>", "<name>", "<type>", transports=[Transport("http")],
            tools=[Tool("<tool_name>", "<description>")])

DMEDServer(card, 8080, lambda name, args: YOUR_FUNCTION(args)).start()
```
