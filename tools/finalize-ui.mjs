import fs from 'node:fs';import path from 'node:path';import {fileURLToPath} from 'node:url';import {connect} from './eez.mjs';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..'),c=await connect(),call=c.call;
async function visit(n,parent){
 if(n.type==='LVGLLabelWidget'){
  let id=n.identifier||'',width=n.rect.width,height=24;
  if(parent?.type==='LVGLButtonWidget')width=parent.rect.width;
  if(id.endsWith('_title')&&['main_title','history_title','equipment_title','settings_title'].includes(id)){width=330;height=32;}
  if(id.endsWith('_clock'))width=145;
  if(id.endsWith('_connection')){width=132;height=22;}
  if(['temperature_caption','humidity_caption','temperature_set','humidity_set'].includes(id))width=150;
  if(['temperature_value','humidity_value'].includes(id)){width=150;height=54;}
  if(id==='main_status'){width=390;height=24;}
  if(id==='trend_title')width=160;
  if(id==='trend_window')width=138;
  if(id.startsWith('device_title'))width=116;
  if(id.startsWith('device_percent')){width=112;height=40;}
  await call('update_widget',{objID:n.objID,props:{width,height,widthUnit:'px',heightUnit:'px'}});
 }
 for(const ch of n.children||[])await visit(ch,n);
}
try{
 for(const p of ['Main','History','Equipment','Settings'])await visit((await call('get_page_tree',{page:p})).root);
 await call('save');console.log(JSON.stringify(await call('build')));
 const r=await call('render_page',{page:'Main'});for(const i of r.content||[])if(i.type==='image')fs.writeFileSync(path.join(root,'artifacts/main-eez.png'),Buffer.from(i.data,'base64'));
 const base=String.raw`C:\Users\hajun\eez-projects\esp32-s3-5inch-monitor`;
 fs.copyFileSync(path.join(base,'esp32-s3-5inch-monitor.eez-project'),path.join(root,'esp32-s3-5inch-monitor.eez-project'));
 fs.cpSync(path.join(base,'src/ui'),path.join(root,'main/ui'),{recursive:true});
}finally{await c.close();}
