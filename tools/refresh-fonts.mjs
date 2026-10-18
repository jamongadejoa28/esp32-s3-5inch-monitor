import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {connect} from './eez.mjs';
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const client = await connect();
try {
  const files = ['assets/font-characters.txt', 'src/ui/dashboard.c', 'src/ui/alarms.c', 'src/domain/settings_catalog.c'];
  let chars = files.map(f => fs.readFileSync(path.join(root, f), 'utf8')).join('');
  const project = JSON.parse(fs.readFileSync(path.join(root, 'esp32-s3-5inch-monitor.eez-project'), 'utf8'));
  chars += JSON.stringify(project.userPages);
  const codes = [...new Set([...chars].map(c => c.codePointAt(0)))].filter(c => c > 127).sort((a,b) => a-b);
  const ranges = '0x20-0x7E,' + codes.map(c => '0x' + c.toString(16)).join(',');
  for (const fontName of ['Body','Small','Title','Value']) await client.call('set_font_ranges', {fontName, ranges});
  await client.call('save');
  const result = await client.call('build');
  console.log(JSON.stringify(result));
  if (!result.ok) throw new Error('EEZ build failed');
  fs.writeFileSync(path.join(root, 'tests/host/glyphs.h'), 'static const unsigned glyph_codes[] = {' + codes.join(',') + '};\n');
} finally { await client.close(); }
