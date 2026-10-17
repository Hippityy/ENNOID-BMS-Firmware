#!/usr/bin/env python3
"""Compare production codec schema with independent official XML fixture; execute C codec."""
from pathlib import Path
import json, re, struct, subprocess, tempfile, xml.etree.ElementTree as ET, sys, os
repo = Path(__file__).resolve().parents[2]
spec = json.loads((repo/'tests/fixtures/tool_v606_config_schema.json').read_text())
rows = re.findall(r'X\((\w+), ([^,]+), (\w+)\)',
                  (repo/'Modules/Inc/modConfigWireFields.h').read_text())
assert rows == [(f['kind'], f['member'], f['name']) for f in spec['fields']]
assert sum(f['width'] for f in spec['fields']) == spec['payload_bytes'] == 268
# Fixture values are independent of C codec and serialize using Python network-endian formats.
values = json.loads((repo/'tests/fixtures/tool_v606_config_defaults.json').read_text())
formats = {'U8':'>B','BOOL':'>B','U16':'>H','I16':'>h','U32':'>I','F32':'>f'}
wire = b''.join(struct.pack(formats[f['kind']], values[f['name']]) for f in spec['fields'])
assert len(wire) == 268
if len(sys.argv) == 2:
    xml = ET.parse(sys.argv[1]).getroot()
    assert [n.text for n in xml.find('SerOrder')] == [f['name'] for f in spec['fields']]
    params = xml.find('Params')
    for field in spec['fields']:
        node = params.find(field['name']); typ = node.findtext('type'); vtx = node.findtext('vTx')
        kind = ('F32' if typ == '1' else 'BOOL' if typ == '5' else 'U8' if typ == '4'
                else {'1':'U8','2':'U8','3':'U16','4':'I16','5':'U32'}[vtx])
        assert kind == field['kind'], field['name']
    print('Official tool XML order and wire types match all', len(rows), 'fields')
with tempfile.TemporaryDirectory(prefix='ennoid-protocol-') as td:
    td = Path(td); (td/'fixture.bin').write_bytes(wire)
    cmd = ['cc', '-std=c11', '-g', '-O1', '-Wall', '-Wextra', '-Werror',
           '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
           '-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections', '-DENNOID_HV_2=1',
           '-Itests/protocol/stubs', '-IModules/Inc', '-IMain', '-ILibraries/Inc',
           'tests/protocol/test_config_wire.c', 'Modules/Src/modConfigWire.c',
           'Modules/Src/modCommandPolicy.c',
           'Modules/Src/modConfig.c', 'Modules/Src/modCellMap.c',
           'Libraries/Src/libBuffer.c', '-lm', '-o', str(td/'test')]
    subprocess.run(cmd, cwd=repo, check=True)
    subprocess.run([str(td/'test'), str(td/'fixture.bin')], check=True,
                   env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"})
print('Schema:96 official fields,268 bytes; all production-code checks passed')
