from pathlib import Path
import hashlib,json
R=Path(__file__).resolve().parent
vendor=R.parent/'vendor'
three=(vendor/'three.min.js').read_text()
assert hashlib.sha256(three.encode()).hexdigest()==json.loads((vendor/'receipt.json').read_text())['three.min.js']['sha256']
data=json.loads((R/'atlas-data.json').read_text())
import base64
assert hashlib.sha256(base64.b64decode(data['base64'])).hexdigest()==data['modelHash']
page=(R/'index.template.html').read_text().replace('/*__CSS__*/',(R/'style.css').read_text()).replace('/*__THREE__*/',three)
page=page.replace('/*__DATA__*/','const ATLAS_DATA = '+json.dumps(data,separators=(',',':')).replace('</','<\\/')+';')
page=page.replace('/*__MODEL__*/',(R/'model.js').read_text())
page=page.replace('/*__APP__*/',(R/'interaction.js').read_text()+'\n'+(R/'scene.js').read_text()+'\n'+(R/'app.js').read_text())
page=page.replace('<!--__LICENSE__-->','<!-- Three.js MIT licence:\n'+(vendor/'THREE-LICENSE.txt').read_text()+'\nOriginal page code: Remmy Victor, GPL-3.0-or-later. Original weights and writing: CC BY 4.0. -->')
assert not any(x in page for x in ['/*__CSS__*/','/*__THREE__*/','/*__DATA__*/','/*__MODEL__*/','/*__APP__*/'])
checks=json.loads((R/'ux-audit/repair-check.json').read_text())
assert checks['passed']
for name,digest in checks['source_sha256'].items():
    assert hashlib.sha256((R/name).read_bytes()).hexdigest()==digest, f'Stale verification for {name}; run node verify-interactions.cjs'
receipt=dict(model=data['modelHash'],html_sha256=hashlib.sha256(page.encode()).hexdigest(),html_bytes=len(page.encode()),all_binary_parameters_embedded=True,native_verification=json.loads((R/'native-verification.json').read_text()),interaction_repair=dict(checks=len(checks['checks']),method=checks['kind'],browser_interaction=checks['browser_interaction'],source_sha256=checks['source_sha256']))
(R/'forge-anatomy.html.tmp').write_text(page)
(R/'forge-anatomy.html.tmp').replace(R/'forge-anatomy.html')
(R/'build-receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
print(json.dumps(receipt))
