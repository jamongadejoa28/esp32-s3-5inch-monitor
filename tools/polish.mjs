import fs from 'node:fs';import path from 'node:path';import {fileURLToPath} from 'node:url';import {connect} from './eez.mjs';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..'),c=await connect(),call=c.call;
function walk(n){return [n,...(n.children||[]).flatMap(walk)];}
async function st(objID,v){await call('set_style',{objID,part:'MAIN',state:'DEFAULT',values:v});}
async function child(parent,type,id,x,y,w,h,props={}){return (await call('create_widget',{parent,type:'LVGL'+type+'Widget',props:{identifier:id,left:x,top:y,width:w,height:h,...props}})).objID;}
try{
 let chars=fs.readFileSync(path.join(root,'assets/font-characters.txt'),'utf8')+'환경 컨트롤러 기본 고급 미확인 화면 예약 시계';
 for(const f of ['src/ui/dashboard.c','src/domain/settings_catalog.c'])chars+=fs.readFileSync(path.join(root,f),'utf8');
 const codes=[...new Set([...chars].map(x=>x.codePointAt(0)))].filter(x=>x>127);
 const ranges='0x20-0x7E,'+codes.map(x=>'0x'+x.toString(16)).join(',');
 for(const fontName of ['Body','Small','Title','Value'])await call('set_font_ranges',{fontName,ranges});
 for(const p of ['Main','History','Equipment','Settings']){
  const nodes=walk((await call('get_page_tree',{page:p})).root);
  for(const n of nodes){
   if(n.type==='LVGLLabelWidget'&&n.text==='Button')await call('delete_widget',{objID:n.objID});
   else if(n.type==='LVGLLabelWidget')await call('update_widget',{objID:n.objID,props:{widthUnit:'px',heightUnit:'px'}});
  }
 }
 // Design-time chip placeholders: runtime replaces them with live state colors.
 let nodes=walk((await call('get_page_tree',{page:'Main'})).root);
 for(let k=0;k<5;k++){
  const n=nodes.find(n=>n.identifier==='device_chips_'+k);if(n.children.length)continue;
  const count=[1,4,5,2,4][k];for(let j=0;j<count;j++){
   const id=await child(n.objID,'Label','preview_chip_'+k+'_'+j,j*25,0,22,24,{text:String(j+1),widthUnit:'px',heightUnit:'px'});
   await st(id,{text_font:'Small',text_color:'#708295',text_align:'CENTER',bg_color:'#E8EDF2',bg_opa:255,radius:6,pad_top:3});
  }
 }
 // Subpage design is editable in EEZ; firmware repopulates the content for live values.
 for(const [page,tabs,rows] of [
  ['History',['15분','1시간','24시간','7일','표 / 그래프'],['온도 -25~85℃ · 습도 0~100%','기록 대기 · RTC 시간과 통신 상태 확인']],
  ['Equipment',['현재상태','경보','스케줄','수동'],['장비 --   |   현재전류 --.- A','송풍   --             냉방 1   --','난방 1   --           가습 1   --','디지털 입력 / 출력   --','누적 가동시간   --']],
  ['Settings',['기본','고급','미확인','화면','예약','시계'],['설정온도                                          --.- ℃','설정습도                                          --.- %','냉방편차                                          --.- ℃','난방편차                                          --.- ℃','가습편차                                          --.- %']]
 ]){
  nodes=walk((await call('get_page_tree',{page})).root);const host=nodes.find(n=>n.identifier===page.toLowerCase()+'_content');
  for(const n of host.children)await call('delete_widget',{objID:n.objID});
  for(let i=0;i<tabs.length;i++){const w=Math.floor(728/tabs.length);const id=await child(host.objID,'Label','preview_'+page.toLowerCase()+'_tab_'+i,16+i*w,16,w-8,38,{text:tabs[i],widthUnit:'px',heightUnit:'px'});await st(id,{text_font:'Body',text_color:'#193047',text_align:'CENTER',bg_color:i?'#E8F1F5':'#D7EDE8',bg_opa:255,radius:8,pad_top:8});}
  for(let i=0;i<rows.length;i++){const id=await child(host.objID,'Label','preview_'+page.toLowerCase()+'_row_'+i,24,80+i*44,700,32,{text:rows[i],widthUnit:'px',heightUnit:'px'});await st(id,{text_font:'Body',text_color:'#708295'});}
 }
 const settings=await call('get_settings');
 const project=JSON.parse(fs.readFileSync(String.raw`C:\Users\hajun\eez-projects\esp32-s3-5inch-monitor\esp32-s3-5inch-monitor.eez-project`,'utf8'));
 const template=project.settings.build.files.find(f=>f.fileName==='ui.c').template.replaceAll('lv_scr_load_anim','lv_screen_load_anim').replaceAll('LV_SCR_LOAD_ANIM_FADE_IN','LV_SCR_LOAD_ANIM_FADE_IN');
 await call('set_build_file_template',{fileName:'ui.c',template});await call('save');console.log(JSON.stringify(await call('build')));
 for(const page of ['Main','History','Equipment','Settings']){
  const r=await call('render_page',{page});for(const i of r.content||[])if(i.type==='image')fs.writeFileSync(path.join(root,'artifacts',page.toLowerCase()+'-eez.png'),Buffer.from(i.data,'base64'));
 }
 fs.copyFileSync(String.raw`C:\Users\hajun\eez-projects\esp32-s3-5inch-monitor\esp32-s3-5inch-monitor.eez-project`,path.join(root,'esp32-s3-5inch-monitor.eez-project'));
 fs.cpSync(String.raw`C:\Users\hajun\eez-projects\esp32-s3-5inch-monitor\src\ui`,path.join(root,'main/ui'),{recursive:true});
 console.log('Polish complete');
}finally{await c.close();}
