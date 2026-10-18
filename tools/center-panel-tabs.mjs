import {connect} from './eez.mjs';
const client = await connect();
const walk = node => [node, ...(node.children || []).flatMap(walk)];
try {
  for (const page of ['History', 'Equipment', 'Settings']) {
    const nodes = walk((await client.call('get_page_tree', {page})).root);
    for (const button of nodes.filter(node => new RegExp(`^${page.toLowerCase()}_tab_\\d+$`).test(node.identifier || ''))) {
      for (const label of button.children.filter(node => node.type === 'LVGLLabelWidget')) {
        await client.call('update_widget', {objID: label.objID, props: {
          left: 0, top: 0, width: button.rect.width, height: 24,
          widthUnit: 'px', heightUnit: 'px', longMode: 'CLIP'
        }});
        await client.call('set_style', {objID: label.objID, part: 'MAIN', state: 'DEFAULT', values: {
          align: 'CENTER', text_align: 'CENTER', pad_top: 0, pad_bottom: 0, pad_left: 0, pad_right: 0
        }});
      }
    }
  }
  await client.call('save');
  const result = await client.call('build');
  console.log(JSON.stringify(result));
  if (!result.ok) throw new Error('EEZ build failed');
} finally { await client.close(); }
