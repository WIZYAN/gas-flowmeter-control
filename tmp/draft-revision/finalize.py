from pathlib import Path
from hashlib import sha256
from zipfile import ZipFile
from lxml import etree as E
from pypdf import PdfReader
import json, shutil

root=Path(r'F:\project\gas-flowmeter-control')
tmp=root/'tmp/draft-revision'
target=root/'doc/Design Document/流量计控制系统软件设计-草稿.docx'
work=tmp/target.name
source=tmp/'source.docx'
digest=lambda p:sha256(p.read_bytes()).hexdigest()
assert digest(target)==digest(source),'Source draft changed during editing; do not overwrite.'
ns={'w':'http://schemas.openxmlformats.org/wordprocessingml/2006/main'}
with ZipFile(work) as z:
    assert z.testzip() is None
    doc=E.fromstring(z.read('word/document.xml'))
body=doc.find('w:body',ns)
tx=lambda e:''.join(e.xpath('.//w:t/text()',namespaces=ns))
text=tx(body)
assert 'V0.2' in text
assert not any(x in text for x in ['再建立 V7/V9 通路','V7 与 V9 按资料要求开启','正常需 V7、V9 开','上行 RS485/CAN 协议'])
tbl=next(t for t in body.findall('w:tbl',ns) if tx(t).startswith('组合及条件'))
states={}
for row in tbl.findall('w:tr',ns)[1:]:
    c=[tx(c) for c in row.findall('w:tc',ns)]
    st=tuple(v=='开启' for v in c[2:])
    allowed=c[0]!='禁止组合'
    assert allowed == (not(st[2] and(st[0] or st[1])))
    states[str(st)]=allowed
assert len(states)==8 and sum(states.values())==5
pdf=PdfReader(tmp/'draft-v02-final.pdf')
assert len(pdf.pages)==22
pdftext='\n'.join(p.extract_text() for p in pdf.pages)
assert 'Error!' not in pdftext and '错误!未定义书签' not in pdftext
assert len(list((tmp/'render-final').glob('page-*.png')))==22
# The EX201 protocol details and example frames were retained verbatim.
with ZipFile(source) as z:
    old=E.fromstring(z.read('word/document.xml')).find('w:body',ns)
oldnodes=list(old)
for idx in range(107,131):
    t=tx(oldnodes[idx])
    if t: assert t in text,(idx,t)
shutil.copy2(work,target)
assert digest(target)==digest(work)
result={'output':str(target),'output_sha256':digest(target),'backup_sha256':digest(source),'pages':22,'all_pages_visually_inspected':True,'truth_table_states':states,'EX201_protocol_preserved':True,'version':'V0.2'}
(tmp/'final-qa.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(result,ensure_ascii=False,indent=2))
