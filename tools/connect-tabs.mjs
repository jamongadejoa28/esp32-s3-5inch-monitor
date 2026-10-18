// Targeted EEZ migration: keeps existing screen shells and sample content.
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {connect} from './eez.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const client = await connect(), call = client.call;
const walk = n => [n, ...(n.children || []).flatMap(walk)];
const style = (objID, values, state = 'DEFAULT') => call('set_style', {objID, part: 'MAIN', state, values});
const specs = [
  ['History', ['15분', '1시간', '24시간', '7일', '그래프 / 표']],
  ['Equipment', ['현재상태', '경보', '스케줄', '수동']],
  ['Settings', ['기본', '고급', '미확인', '화면', '예약', '시계']]
];
try {
  const {actions} = await call('list_actions');
  for (const [page, labels] of specs) {
    const prefix = page.toLowerCase(), action = prefix + '_tab';
    if (!actions.some(a => a.name === action)) await call('create_action', {name: action, implementationType: 'native'});
    let nodes = walk((await call('get_page_tree', {page})).root);
    const host = nodes.find(n => n.identifier === prefix + '_content');
    if (!host) throw new Error('Missing content panel: ' + page);
    // The toolbar stays fixed while only the body is rebuilt by the C controller.
    await call('update_widget', {objID: host.objID, props: {widgetFlags: ''}});
    let body = nodes.find(n => n.identifier === prefix + '_body');
    if (!body) {
      body = await call('create_widget', {parent: host.objID, type: 'LVGLPanelWidget', props: {
        identifier: prefix + '_body', left: 20, top: 62, width: 720,
        height: page === 'History' ? 280 : 270, widthUnit: 'px', heightUnit: 'px',
        widgetFlags: 'SCROLLABLE|SCROLL_ELASTIC|SCROLL_MOMENTUM'
      }});
      await style(body.objID, {bg_color: '#FFFFFF', bg_opa: 255, border_width: 0, radius: 12,
        pad_left: 0, pad_right: 0, pad_top: 0, pad_bottom: 0});
      for (const n of host.children) {
        if (n.identifier?.startsWith('preview_' + prefix + '_tab_')) continue;
        await call('move_widget', {objID: n.objID, newParent: body.objID});
        await call('update_widget', {objID: n.objID, props: {left: n.rect.left - 20, top: n.rect.top - 62}});
      }
    }
    for (const n of nodes.filter(n => n.identifier?.startsWith('preview_' + prefix + '_tab_')))
      await call('delete_widget', {objID: n.objID});
    for (let i = 0; i < labels.length; i++) {
      const id = `${prefix}_tab_${i}`;
      let button = nodes.find(n => n.identifier === id);
      const x = page === 'Settings' ? 12 + i * 124 : page === 'Equipment' ? 16 + i * 185 : i === 4 ? 504 : 16 + i * 120;
      const w = page === 'Settings' ? 116 : page === 'Equipment' ? 172 : i === 4 ? 236 : 110;
      if (!button) button = await call('create_widget', {parent: host.objID, type: 'LVGLButtonWidget', props: {
        identifier: id, left: x, top: 10, width: w, height: 44, widthUnit: 'px', heightUnit: 'px',
        widgetFlags: 'CLICKABLE|CLICK_FOCUSABLE', states: i === 0 ? 'CHECKED' : ''
      }});
      await style(button.objID, {bg_color: '#E8F1F5', bg_opa: 255, border_width: 0, radius: 10, shadow_width: 0,
        pad_top: 0, pad_bottom: 0, pad_left: 0, pad_right: 0, text_color: '#193047'});
      await style(button.objID, {bg_color: '#0E9482', text_color: '#FFFFFF'}, 'CHECKED');
      await style(button.objID, {bg_color: '#BEDFD9'}, 'PRESSED');
      await style(button.objID, {bg_color: '#087D70'}, 'CHECKED|PRESSED');
      await call('bind_event', {objID: button.objID, event: 'CLICKED', action, userData: i});
      nodes = walk((await call('get_page_tree', {page})).root);
      const current = nodes.find(n => n.objID === button.objID);
      let label = current.children.find(n => n.type === 'LVGLLabelWidget');
      if (!label) label = await call('create_widget', {parent: button.objID, type: 'LVGLLabelWidget', props: {text: labels[i]}});
      await call('update_widget', {objID: label.objID, props: {
        text: labels[i], left: 0, top: 11, width: w, height: 24, widthUnit: 'px', heightUnit: 'px', widgetFlags: ''
      }});
      await style(label.objID, {text_font: 'Body', text_align: 'CENTER'});
    }
    console.log(page + ': native actions bound to ' + labels.length + ' tabs');
  }
  await call('save');
  const result = await call('build');
  console.log(JSON.stringify(result));
  fs.writeFileSync(path.join(root, 'artifacts/eez-tabs-build.json'), JSON.stringify(result, null, 2));
  for (const [page] of specs) {
    const tree = await call('get_page_tree', {page});
    fs.writeFileSync(path.join(root, `artifacts/${page.toLowerCase()}-tree.json`), JSON.stringify(tree, null, 2));
  }
} finally {
  await client.close();
}
