// Targeted changes in the EEZ source project, followed by normal generation.
import fs from 'node:fs';
import {connect} from './eez.mjs';
const client = await connect();
const walk = node => [node, ...(node.children || []).flatMap(walk)];
try {
  const info = await client.call('get_project_info');
  const project = JSON.parse(fs.readFileSync(info.filePath, 'utf8'));
  const file = project.settings.build.files.find(file => file.fileName === 'ui.c');
  if (!file) throw new Error('Missing ui.c generation template');
  const template = file.template.replace(
    /lv_(?:scr|screen)_load_anim\(screen,\s*LV_(?:SCR|SCREEN)_LOAD_ANIM_FADE_IN,\s*200,\s*0,\s*false\);/g,
    'lv_screen_load(screen);');
  if (template.includes('LOAD_ANIM_FADE')) throw new Error('Unexpected fade template; inspect before changing');
  await client.call('set_build_file_template', {fileName: 'ui.c', template});
  for (const page of ['Main', 'History', 'Equipment', 'Settings']) {
    const nodes = walk((await client.call('get_page_tree', {page})).root);
    const clock = nodes.find(node => node.identifier === page.toLowerCase() + '_clock');
    if (!clock) throw new Error('Missing clock: ' + page);
    await client.call('update_widget', {objID: clock.objID, props: {
      left: 390, width: 245, height: 24, widthUnit: 'px', heightUnit: 'px',
      text: '----/--/-- --:-- KST'
    }});
  }
  await client.call('save');
  const result = await client.call('build');
  console.log(JSON.stringify(result));
  if (!result.ok) throw new Error('EEZ build failed');
} finally {
  await client.close();
}
