import json,pathlib
fields=[]
def add(group,ko,en,address=0,lo=0,hi=1,scale=1,local='',locked=False):fields.append(dict(group=group,ko=ko,en=en,address=address,minimum=lo,maximum=hi,scale=scale,local=local,stopped=locked))
for args in [('설정온도','Set temperature',40012,50,500),('설정습도','Set humidity',40013,200,800),('냉방편차','Cooling deviation',40014,1,100),('난방편차','Heating deviation',40015,1,100),('가습편차','Humidify deviation',40016,10,100),('제습편차','Dehumidify deviation',40017,10,100)]:add(0,*args,10)
add(0,'운전지연','Start delay',40018,1,180);add(0,'정지지연','Stop delay',40019,1,180)
for i,(ko,en) in enumerate([('빠른 온도 1','Quick temperature 1'),('빠른 온도 2','Quick temperature 2'),('빠른 습도 1','Quick humidity 1'),('빠른 습도 2','Quick humidity 2')]):add(0,ko,en,0,50 if i<2 else 200,500 if i<2 else 800,10,'quick'+str(i))
for i,(ko,en) in enumerate([('냉방 표시모드','Cooling display mode'),('난방 표시모드','Heating display mode'),('가습 표시모드','Humidify display mode'),('제습 표시모드','Dehumidify display mode')]):add(1,ko,en,0,0,1,1,'mode'+str(i))
for ko,en,a,lo,hi,scale in [('온도보정','Temperature calibration',40021,-100,100,10),('습도보정','Humidity calibration',40022,-300,300,10),('냉방시 가습금지','Block humidify on cooling',40053,0,1,1),('난방시 제습금지','Block drying on heating',40054,0,1,1),('실린더 오염시간','Cylinder service hours',40059,1000,20000,1),('고온경보 / 금지 24.9','High temp / off 24.9',40027,249,500,10),('저온경보 / 금지 4.9','Low temp / off 4.9',40028,49,200,10),('고습경보 / 금지 59.9','High humidity / off 59.9',40029,599,900,10),('저습경보 / 금지 9.9','Low humidity / off 9.9',40030,99,400,10)]:add(1,ko,en,a,lo,hi,scale,locked=True)
for ko,en in [('운전모드','Equipment type'),('장비번호 변경','Controller address write'),('원격접점 모드','Remote contact mode'),('냉방 스텝 / 비례 설정','Cooling stages / proportional'),('냉방 비례주기','Cooling proportional cycle'),('냉방 운전지연','Cooling start delay'),('냉방 정지지연','Cooling stop delay'),('냉방 재가동지연','Cooling restart delay'),('난방 스텝 / 비례 설정','Heating stages / proportional'),('난방 비례주기','Heating proportional cycle'),('가습 스텝 / 비례 설정','Humidify stages / proportional'),('가습 비례주기','Humidify proportional cycle'),('배수시간','Drain duration'),('배수주기','Drain interval'),('가습기 방식','Humidifier type'),('가습기 전압','Humidifier voltage'),('가습기 용량','Humidifier capacity'),('가습기 전류보정','Humidifier current calibration'),('전극봉 적산 초기화','Controller cylinder hours reset'),('제습 스텝 / 비례 설정','Drying stages / proportional'),('냉방 경보지연','Cooling alarm delay'),('비례 출력 테스트','Analog output test')]:add(2,ko,en,local='unsupported',locked=True)
add(3,'화면밝기','Display brightness',0,1,100,1,'brightness')
add(3,'조명시간 / 0: 계속','Sleep minutes / 0: never',0,0,10,1,'sleep')
add(3,'언어 / 0: 한국어 1: English','Language / 0: Korean 1: English',0,0,1,1,'language')
add(3,'연결 주소','Connection address',0,1,99,1,'unit')
add(3,'바로가기 / 0:설정 1:입출력 2:경보','Shortcut / 0:Settings 1:I/O 2:Alarms',0,0,2,1,'shortcut')
for d,ko in enumerate(['일','월','화','수','목','금','토']):
 add(4,ko+'요일 운전 (HHMM)',['Sun','Mon','Tue','Wed','Thu','Fri','Sat'][d]+' start (HHMM)',0,0,2359,1,'on'+str(d))
 add(4,ko+'요일 정지 (HHMM)',['Sun','Mon','Tue','Wed','Thu','Fri','Sat'][d]+' stop (HHMM)',0,0,2359,1,'off'+str(d))
for ko,en,key,lo,hi in [('년','Year','year',2024,2099),('월','Month','month',1,12),('일','Day','day',1,31),('시','Hour','hour',0,23),('분','Minute','minute',0,59)]:add(5,ko,en,0,lo,hi,1,key)
p=pathlib.Path('assets/settings.json');p.write_text(json.dumps(fields,ensure_ascii=False,indent=2))
out=['#include "domain/settings_catalog.h"','const setting_field_t setting_fields[]={']
for f in fields:out.append('{'+','.join([str(f['group']),json.dumps(f['ko'],ensure_ascii=False),json.dumps(f['en']),str(f['address']),str(f['minimum']),str(f['maximum']),str(f['scale']),json.dumps(f['local']),str(f['stopped']).lower()])+'},')
out+=['};','const size_t setting_count = sizeof(setting_fields)/sizeof(setting_fields[0]);']
pathlib.Path('src/domain/settings_catalog.c').write_text('\n'.join(out)+'\n')
