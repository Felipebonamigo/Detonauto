#!/bin/bash
# uso: mcp.sh <method> '<params json>'
U=http://127.0.0.1:8010/mcp
H=(-H 'Content-Type: application/json' -H 'Accept: application/json, text/event-stream')
if [ ! -s /tmp/mcp/sid ]; then
  curl -s -D /tmp/mcp/hdr -o /dev/null -X POST $U "${H[@]}" -d '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-03-26","capabilities":{},"clientInfo":{"name":"claude","version":"1"}}}'
  grep -i '^Mcp-Session-Id' /tmp/mcp/hdr | cut -d' ' -f2 | tr -d '\r\n' > /tmp/mcp/sid
  curl -s -X POST $U "${H[@]}" -H "Mcp-Session-Id: $(cat /tmp/mcp/sid)" -d '{"jsonrpc":"2.0","method":"notifications/initialized"}' >/dev/null
fi
curl -s -X POST $U "${H[@]}" -H "Mcp-Session-Id: $(cat /tmp/mcp/sid)" -d "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"$1\",\"params\":$2}"
