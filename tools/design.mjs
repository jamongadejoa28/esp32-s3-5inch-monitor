import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {connect} from './eez.mjs';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const c=await connect(), call=c.call;
const bg='#F3F6FA', ink='#193047', muted='#708295', teal='#0E9482', border='#E2E9F0';
const tree=await call('get_page_tree',{page:'Main'});
const target=String.raw`C:\Users\hajun\eez-projects\esp32-s3-5inch-monitor\esp32-s3-5inch-monitor.eez-project`;
if(!fs.existsSync(path.join(root,'artifacts/original.eez-project')))fs.copyFileSync(target,path.join(root,'artifacts/original.eez-project'));
const catalog=JSON.parse(fs.readFileSync(path.join(root,'assets/settings.json'),'utf8'));
const strings='대시보드 이력 장비 설정 현재온도 현재습도 설정값 실시간 추세 최근 분 송풍 냉방 난방 가습 제습 호 운전 정지 대기 경보 통신 연결 확인 중 마지막 수신 바로가기 빠른설정 변경 저장 취소 적용 실패 확인불가 비례 표시 계산값 단계 시간 날짜 전원 사용 금지 미지원 정상 발생 해제 급수 배수 초기화 입출력 누적 가동시간 모니터 전문가 예약 현재 세션 내장 저장 온습도 최대 최소 평균 삭제 전체 일주일 이전 다음 표 그래프 값 편차 ℃ % 분 초 시 년 월 일 0123456789 -- : . / 원격 실제 미확인 기능은 변경할 수 없습니다 수동모드 경고 연결된 컨트롤러를 정지한 후 사용하세요 한국어 영어 화면 밝기 미설정 정보 동기화 스케줄 비활성 운전중 정보 표시모드 적용됨 전송중 통신끊김 값이 오래되었습니다 요청거부 적용확인불가 장비 정지 필요 확인 온습도 동작 상태 메모리 진단 현재전류 디지털 입력 디지털 출력 적산 리셋 보관 보정 범위 모델 판별없이 사용자 선택 안내 화면표시 실제출력 아님 센서 송풍기 냉방기 난방기 가습기 화재 누수 실린더 오염 급수 배수 고온 저온 고습 저습 전극봉  '+catalog.map(x=>x.ko).join(' ');
const codes=[...new Set([...strings].map(x=>x.codePointAt(0)))].filter(x=>x>127);
const ranges='0x20-0x7E,'+codes.map(x=>'0x'+x.toString(16)).join(',');
const fonts=await call('list_fonts');
fs.writeFileSync(path.join(root,'assets/font-characters.txt'),strings);
try{
 for(const [name,size] of [['Body',18],['Small',14],['Title',24],['Value',40]]){
  const exists=JSON.stringify(fonts).includes('"'+name+'"');
  if(!exists)await call('add_font',{name,size,bpp:4,filePath:String.raw`\\wsl.localhost\Ubuntu-24.04\usr\share\fonts\truetype\nanum\NanumGothic.ttf`,ranges});
 }
 for(const name of ['fan','cool','heat','humid','dry']){
  const b=await call('list_bitmaps');
  if(!JSON.stringify(b).includes('"'+name+'"'))await call('add_bitmap',{name,filePath:path.join(root,'assets/icons',name+'_00.png')});
 }
 const pages=await call('list_pages');
 const names=['Main','History','Equipment','Settings'];
 const actions=['nav_main','nav_history','nav_equipment','nav_settings','power','shortcut','quick_temp','quick_hum'];
 const oldActions=JSON.stringify(await call('list_actions'));
 for(const name of actions)if(!oldActions.includes('"'+name+'"'))await call('create_action',{name});
 async function style(objID,values){if('pad_all' in values){let v=values.pad_all;delete values.pad_all;Object.assign(values,{pad_top:v,pad_bottom:v,pad_left:v,pad_right:v});}await call('set_style',{objID,part:'MAIN',state:'DEFAULT',values});}
 async function widget(page,type,id,x,y,w,h,extra={},styles={}){
  const r=await call('create_widget',{page,type:'LVGL'+type+'Widget',props:{identifier:id,left:x,top:y,width:w,height:h,...extra}});
  await style(r.objID,{text_font:'Body',text_color:ink,...styles});return r.objID;
 }
 const label=(p,id,t,x,y,w,h=24,styles={})=>widget(p,'Label',id,x,y,w,h,{text:t,longMode:'CLIP'},{...styles});
 const panel=(p,id,x,y,w,h)=>widget(p,'Panel',id,x,y,w,h,{}, {bg_color:'#FFFFFF',bg_opa:255,radius:16,border_width:1,border_color:border,pad_all:0});
 async function button(p,id,t,x,y,w,h,action,primary=false){
  const b=await widget(p,'Button',id,x,y,w,h,{}, {bg_color:primary?teal:'#FFFFFF',bg_opa:255,radius:12,border_width:primary?0:1,border_color:border,shadow_width:0,pad_all:0});
  const l=await call('create_widget',{parent:b,type:'LVGLLabelWidget',props:{left:0,top:(h-22)/2,width:w,height:24,text:t,longMode:'CLIP'}});await style(l.objID,{text_font:'Body',text_color:primary?'#FFFFFF':ink,text_align:'CENTER'});
  if(action)await call('bind_event',{objID:b,event:'CLICKED',action});return b;
 }
 for(const [idx,p] of names.entries()){
  if(!JSON.stringify(pages).includes('"'+p+'"'))await call('create_page',{name:p,width:800,height:480});
  const t=await call('get_page_tree',{page:p});
  for(const child of t.root.children)await call('delete_widget',{objID:child.objID});
  await style(t.root.objID,{bg_color:bg,bg_opa:255,text_font:'Body',text_color:ink,pad_all:0});
  await label(p,p.toLowerCase()+'_title', ['환경 대시보드','데이터 이력','장비 상태','운전 설정'][idx],20,17,280,30,{text_font:'Title'});
  await label(p,p.toLowerCase()+'_clock','--:--',490,20,140,24,{text_color:muted,text_align:'RIGHT'});
  await label(p,p.toLowerCase()+'_connection','연결 확인 중',650,22,132,22,{text_font:'Small',text_color:muted,text_align:'RIGHT'});
  for(let n=0;n<4;n++)await button(p,p.toLowerCase()+'_nav_'+n,['대시보드','이력','장비','설정'][n],20+n*195,426,175,44,actions[n],n===idx);
  console.log('Page shell: '+p);
 }
 await panel('Main','temperature_card',20,66,180,132);
 await label('Main','temperature_caption','현재온도',36,80,145,22,{text_color:muted,text_font:'Small'});
 await label('Main','temperature_value','--.-',34,105,145,52,{text_font:'Value'});
 await label('Main','temperature_set','설정 --.- ℃',36,165,145,24,{text_color:'#188BD5'});
 await panel('Main','humidity_card',214,66,180,132);
 await label('Main','humidity_caption','현재습도',230,80,145,22,{text_color:muted,text_font:'Small'});
 await label('Main','humidity_value','--.-',228,105,145,52,{text_font:'Value'});
 await label('Main','humidity_set','설정 --.- %',230,165,145,24,{text_color:teal});
 await panel('Main','trend_card',408,66,372,132);
 await label('Main','trend_title','실시간 추세',424,78,160,22,{text_font:'Small'});
 await label('Main','trend_window','최근 15분',624,78,138,22,{text_font:'Small',text_color:muted,text_align:'RIGHT'});
 await widget('Main','Panel','main_chart_host',424,108,338,74,{}, {bg_opa:0,border_width:0,pad_all:0});
 for(let k=0;k<5;k++){
  let x=20+k*154;
  await panel('Main','device_card_'+k,x,214,144,151);
  await label('Main','device_title_'+k,['송풍','냉방','난방','가습','제습'][k],x+14,227,115,24);
  await widget('Main','Image','device_icon_'+k,x+44,254,56,56,{image:['fan','cool','heat','humid','dry'][k]});
  await label('Main','device_percent_'+k,'',x+16,267,112,40,{text_font:'Title',text_align:'CENTER'});
  await widget('Main','Panel','device_chips_'+k,x+8,324,128,27,{}, {bg_opa:0,border_width:0,pad_all:0});
 }
 await label('Main','main_status','컨트롤러 데이터 동기화 대기',20,384,350,24,{text_font:'Small',text_color:muted});
 await button('Main','shortcut_button','바로가기',420,377,116,38,'shortcut');
 await button('Main','power_button','운전 / 정지',552,377,228,38,'power',true);
 await panel('History','history_content',20,66,760,346);
 await panel('Equipment','equipment_content',20,66,760,346);
 await panel('Settings','settings_content',20,66,760,346);
 const settings=await call('get_settings');
 // Fix legacy API in no-flow template while retaining EEZ generated sections.
 const files=await call('list_build_files');
 fs.writeFileSync(path.join(root,'artifacts/build-files.json'),JSON.stringify(files,null,2));
 await call('update_settings',{build:{destinationFolder:'src\\ui',lvglInclude:'lvgl.h'}});
 await call('save');
 const result=await call('build');console.log(JSON.stringify(result));
 const r=await call('render_page',{page:'Main'});
 for(const i of r.content||[])if(i.type==='image')fs.writeFileSync(path.join(root,'artifacts/dashboard-eez.png'),Buffer.from(i.data,'base64'));
 fs.copyFileSync(target,path.join(root,'esp32-s3-5inch-monitor.eez-project'));
 console.log('Design saved and built');
}finally{await c.close();}
