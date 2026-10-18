// Run with Windows Node: the EEZ bridge and handshake live on Windows.
import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { StdioClientTransport } from '@modelcontextprotocol/sdk/client/stdio.js';
export async function connect() {
  const h = JSON.parse(fs.readFileSync(path.join(os.tmpdir(), 'eez-studio-mcp-bridge.json'), 'utf8'));
  const server = process.env.EEZ_MCP_SERVER || String.raw`\\wsl.localhost\Ubuntu-24.04\home\hajun\dev_ws\esp32-s3-wowki\.tools\eez-studio-mcp-server\mcp-server\dist\index.js`;
  const c = new Client({name:'mex-monitor-authoring',version:'1.0.0'});
  await c.connect(new StdioClientTransport({command:process.execPath,args:[server],env:{...process.env,EEZ_MCP_BRIDGE_PORT:String(h.port),EEZ_MCP_BRIDGE_TOKEN:h.token},stderr:'pipe'}));
  const call = async (name,args={}) => {
    const r=await c.callTool({name,arguments:args},undefined,{timeout:120000});
    if(r.isError) throw new Error(name+': '+r.content?.find(x=>x.type==='text')?.text);
    if(r.structuredContent) return r.structuredContent;
    const t=r.content?.find(x=>x.type==='text');
    if(t) {try{return JSON.parse(t.text);}catch{ return r; }}
    return r;
  };
  const info=await call('get_project_info');
  if(!info.filePath.replaceAll('\\','/').endsWith('/esp32-s3-5inch-monitor/esp32-s3-5inch-monitor.eez-project')) {await c.close();throw Error('Open the target 5inch project first');}
  return {call,close:()=>c.close()};
}
if(process.argv[1]?.endsWith('eez.mjs')) {
 const c=await connect();
 try {
  const r=await c.call(process.argv[2]||'get_project_info',JSON.parse(process.argv[3]||'{}'));
  for(const item of r.content||[]) if(item.type==='image') {fs.writeFileSync(new URL('../artifacts/eez-preview.png',import.meta.url),Buffer.from(item.data,'base64'));delete item.data;}
  console.log(JSON.stringify(r));
 } finally{await c.close();}
}
