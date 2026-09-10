from pathlib import Path
from copy import deepcopy
from zipfile import ZipFile, ZIP_DEFLATED
from hashlib import sha256
import json, math, shutil
from lxml import etree as ET
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(r'F:\project\gas-flowmeter-control')
TMP = ROOT / 'tmp/draft-revision'
TARGET = ROOT / 'doc/Design Document/流量计控制系统软件设计-草稿.docx'
SOURCE = TMP / 'source.docx'
OUT = TMP / TARGET.name
if not SOURCE.exists():
    shutil.copy2(TARGET, SOURCE)
assert sha256(SOURCE.read_bytes()).hexdigest() == '3f8d204e6aa5d31f5955841522810eb473f3813efb95a6707881094620512f31'
with ZipFile(SOURCE) as z:
    parts = {n: z.read(n) for n in z.namelist()}
W = 'http://schemas.openxmlformats.org/wordprocessingml/2006/main'
NS = {'w': W, 'wp': 'http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing', 'a': 'http://schemas.openxmlformats.org/drawingml/2006/main'}
def tag(s): return '{'+W+'}'+s
root = ET.fromstring(parts['word/document.xml'])
body = root.find('w:body', NS)
nodes = list(body)
assert len(nodes) == 180

def textof(e): return ''.join(e.itertext()) if False else ''.join(e.xpath('.//w:t/text()', namespaces=NS))
def setp(e, text):
    ts = e.findall('.//w:t', NS)
    if ts:
        ts[0].text = text
        ts[0].set('{http://www.w3.org/XML/1998/namespace}space', 'preserve')
        for t in ts[1:]: t.text = ''
    else:
        r = ET.SubElement(e, tag('r'))
        t = ET.SubElement(r, tag('t')); t.text = text

def para(i, text): setp(nodes[i], text)
def insert_after(i, texts):
    anchor = nodes[i]
    for text in texts:
        p = deepcopy(nodes[52])
        for e in p.findall('.//w:bookmarkStart',NS)+p.findall('.//w:bookmarkEnd',NS): e.getparent().remove(e)
        setp(p, text)
        anchor.addnext(p); anchor = p

def cells(row): return row.findall('w:tc', NS)
def setcell(c, text):
    ps = c.findall('w:p', NS)
    if not ps: ps = [ET.SubElement(c, tag('p'))]
    setp(ps[0], text)
    for p in ps[1:]: c.remove(p)
def rowset(row, vals):
    assert len(cells(row)) == len(vals)
    for c, v in zip(cells(row), vals): setcell(c, v)
def rowmatch(i, key):
    return next(r for r in nodes[i].findall('w:tr',NS) if textof(cells(r)[0]) == key)
def editrow(i, key, vals): rowset(rowmatch(i,key), vals)
def addrow(i, vals):
    row = deepcopy(nodes[i].findall('w:tr',NS)[-1])
    for e in row.findall('.//w:bookmarkStart',NS)+row.findall('.//w:bookmarkEnd',NS): e.getparent().remove(e)
    rowset(row, vals); nodes[i].append(row)
def rebuild(i, rows):
    trs = nodes[i].findall('w:tr',NS)
    proto = deepcopy(trs[1])
    for r in trs[1:]: nodes[i].remove(r)
    for vals in rows:
        r = deepcopy(proto); rowset(r,vals); nodes[i].append(r)

para(15, '时间：  2026.09.10   V0.2')
revrows = nodes[20].findall('w:tr',NS)
blankrev = next(r for r in revrows[3:] if len(cells(r)) == 4 and not textof(r).strip())
rowset(blankrev, ['V0.2', '待填写', '2026.09.10', '明确 CAN 与以太网上行接口；修订阀门互斥、功耗及温度要求'])

paragraphs = {
52: '本文定义基于 R7FA4M1AB3CFM 的六路质量流量计控制系统初步软件方案，用于硬件接口核对、固件开发和联调。MCU 通过 RJ45 连接的 RS485 总线向六台 EX-201S 系列质量流量控制器下发流量设定并读取运行参数，同时管理九个外部电磁阀。上位机通过 CAN 和以太网两种接口控制 MCU，并读取当前设备状态、阀门状态及流量计参数；以太网使用独立 RJ45 网口。[S6]',
53: '初步方案采用一条 MFC 下行总线、六个独立通道对象和统一阀门联锁。MFC 内部完成流量闭环，MCU 负责设定、监测与过程协调。上位机可经 CAN 或以太网完成检测与过程协调，MCU 统一校验并执行请求，始终保留本地阀门联锁和故障关断职责。已知要求、设计建议及待确认参数在各节分别注明。',
55: '气路由六条支路经流量计汇入混气管，其中五路为常规使用气体，第六路预留。V1 至 V6 为支路阀，混气管后配置 V7、V8、V9，用于正常输出和排空、校准或测试气体通路。V7、V8 中任一阀开启时，V9 不可开启；V9 开启时，V7、V8 均不可开启，形成互斥。各允许组合与具体工艺模式的对应关系另行确认。[S3][S6]',
56: '项目核心约束为：采用瑞萨 R7FA4M1AB3CFM，48 MHz Arm Cortex-M4F 内核，256 KB Code Flash、8 KB Data Flash、32 KB SRAM；系统待机功耗必须小于 100 W；产品应满足长期可靠性要求，在 −10 ℃ 至 +40 ℃ 环境温度下稳定工作。功耗按项目指标验收，测量边界和条件需冻结；系统环境指标与 MFC 自身允许温度的适配见第 2.3 节。[S5][S6]',
69: '六台 MFC 使用独立地址接入同一条半双工 RS485 总线，由 MCU 逐台访问。软件维持设定流量、实际流量、气体类型、满量程、内部阀状态、设备报警和通讯状态。V1 至 V6 与对应 MFC 成对管理，V7 至 V9 统一联锁。CAN 与以太网作为独立上行接口，共用同一业务接口和状态缓存，上位机请求经 MCU 转换为下行事务。',
76: '建议六路流量刷新周期不大于 1 s，内部阀状态及 MFC 报警刷新不大于 2 s，需在 9600 bps 下实测。外部阀联锁在每次请求和输出更新前本地执行；上行接收、执行、完成与实际关断时间分别记录并冻结。MFC 典型响应约 1 s 属于仪表动态特性，不等同于指令完成或混气稳定时间。[S2]',
80: 'MCU 提供 4 路 SCI/UART 和 1 路 CAN，无片内以太网控制器。[S5] 六台 MFC 共用 SCI2。以太网须补充外接控制器或模块及所需 PHY、隔离磁性器件和独立 RJ45 网口，具体方案见第 4.5 节；仅加 PHY 或改用现有 RS485 RJ45 不能实现以太网。',
85: 'MFC 下行路径采用原理图第 3 页的隔离收发电路及 J9/J15 RJ45 接口，按一主多从总线布线。J9 与 J15 是同一 RS485 总线的连接点。六台设备沿总线主干接入，终端电阻按总线两端配置。上行 CAN、上行以太网与下行 RS485 分开布线、标识和管理，新增以太网口编号待硬件确认。[S4][S6]',
86: '阀号和气路位置参考 S3，V7/V8 与 V9 的互斥逻辑以 S6 最新要求为准。外部阀按实际接线与 GPIO 建立映射。机械手动按钮、急停、阀位或驱动反馈的接入尚待硬件确认，不能用软件命令位代替实际阀位。',
91: 'CAN / 以太网接收 → 协议适配 → 控制权与参数校验 → 整机状态机 → 通道事务及阀门仲裁 → EX201 协议与总线调度 / GPIO 驱动。设备响应更新统一状态缓存，经 CAN 或以太网返回上位机。上位机可计算配比、检查状态并组织流程；MCU 对每次执行独立检查许可、互斥与数据有效性，所有命令入口均不得直接改写阀门 GPIO。',
92: '串口、CAN 及以太网相关中断仅处理必要的收发、事件和时间戳，不执行整机状态切换或 Flash 擦写。主循环优先处理硬件停机条件与阀门联锁，再处理收发解析、阀门计时、事务、轮询、上行请求和后台存储。各任务及队列均设上限；网络突发流量不得阻塞本地保护和 RS485 调度，队列满时返回忙或丢弃低优先级遥测并计数。',
102: '下行板端与设备端应按 TR+/TR−及实际收发器极性逐针核对，不能只凭 A/B 或 H/L 名称推断。MFC RJ45 使用专用 RS485 线束，不连接以太网交换机，不使用 PoE 供电；与上行以太网 RJ45 采用明确且不同的接口标识。TR_COM、屏蔽及 GND4/GND5 的参考关系需在线束图中明确。[S2][S4]',
137: '启动前确认参与通道启用、气体和量程匹配、预热完成、无锁定故障、手动许可有效，并选定经工艺批准的公共通路组合。保持 V1 至 V6 关闭，逐台确认 WFSM=0、WVSS=2；写入目标 WSFD 并回读 RSFD，再按第 4.4 节先关后开建立合法的 V7/V8 或 V9 通路，依次切到 WVSS=1 并开启参与支路。通路模式尚未批准时禁止启动；任一步失败均进入停止流程。',
139: '正常停止先关闭参与支路的外部阀，同时排队发送 WVSS=2 和 WSFD=0000，再按已确认的关断时序关闭当前开启的 V7 至 V9，使所有外部阀最终处于关闭状态。故障停止立即撤销全部外部开阀输出，再尽力关闭各 MFC；通讯不可用时仍完成外部关断。停止不自动排空，泄压和对应路径由独立工艺流程定义。',
147: 'GPIO 网络和引脚来自 S4 第 2 页；支路阀用途参考 S3，公共阀模式以 S6 互斥规则和后续批准的气路说明为准。实际装配需逐路通断验证。驱动链为 MCU、2N7002 前级、光耦及后级功率器件。软件提供逻辑 OPEN/CLOSE，GPIO 有效极性、上电默认态及线圈失电状态由硬件确认后写入 BSP。',
148: '互斥规则与维护模式',
151: '表中“允许”仅指满足 V7/V8 与 V9 的互斥关系，不代表该组合已获正常输出、排空或校准的工艺许可。判定条件为 NOT [V9 AND (V7 OR V8)]；V7、V8 同时开启在该规则下允许，但并非所有模式都要求同时开启。S3 中与此规则冲突的旧开阀组合不再作为执行依据。[S6]',
152: '每次开阀先检查当前命令状态、正在执行的动作及目标组合；禁止组合直接拒绝并返回联锁原因。维护模式逐个测试九阀，同样执行互斥，不能绕过急停或手动许可。进入维护前整机停机，点动超时、退出维护或许可撤销时关闭输出；点动时限待确认。',
156: 'V7/V8 组与 V9 之间切换采用先关后开：隔离进气并将设定归零 → 关闭旧通路 → 等待关断确认或经验证的最坏关闭时间 → 复核互斥和工艺许可 → 开启新通路。阀门管理器串行执行整组事务，包含已接受但尚未输出的开阀动作；新停止请求取消未完成的开启动作，迟到命令不得重新开阀。无反馈时须显式标记实际阀位未确认。',
158: '上位机同时支持 CAN 和以太网两种通讯接口；以太网经独立 RJ45 网口连接，CAN 使用独立收发器和连接端口。两种上行链路共用业务命令、状态缓存与错误码，支持 MCU 控制、设备状态读取、阀门状态读取、MFC 参数读取，以及上位机检测和过程协调。下行仍由 MCU 作为唯一 RS485 主站执行 EX201 ASCII 协议。[S6]',
161: '上位机总览显示整机模式、控制权来源、六路气体、实际流量、目标与已确认流量、满量程、通讯状态、报警及 V1 至 V9 输出命令。每项数据带有效性和更新时间；内部阀读回状态与外部阀命令状态分开显示，有物理反馈时另报反馈值。远程心跳超时、CAN bus-off 或以太网连接中断均上报链路状态；当前控制会话失效时首版建议受控停止，禁止另一接口自动接管并重放旧命令，阈值和最终策略待确认。',
163: '首版使用片内 8 KB Data Flash 保存低频配置。参数包括版本、序号、CRC、通道启用、地址、气体许可、单位与量程核对值、通讯超时、流量偏差阈值、阀门时序，以及确认后的 CAN 节点和以太网地址配置。实际流量、非零运行输出和远程控制会话不作为上电恢复依据。',
165: '事件日志采用 RAM 环形缓冲，记录单调时间、事件号、通道、命令来源、会话及请求号、旧值、新值和失败原因。跨断电日志及绝对时间另行确认存储和时钟来源。建议静态 RAM 预算不超过 24 KB，至少预留 8 KB 给栈与余量；以太网收发缓存、协议栈及 CAN 分段缓冲必须纳入该预算，最终以链接映射和栈高水位实测核算。',
174: '联调按电气与单机通讯、六路总线、九阀空载联锁、CAN 与以太网分别联调、双接口并发、实际流量及环境功耗验证的顺序推进。保存固件与原理图版本、设备铭牌、串口/CAN/网络抓包、GPIO 时序、测试仪器和结果；分别记录数据刷新、请求确认、阀门输出、物理关断及计量误差。',
178: '优先完成 D01、D07 至 D11 的实物和接线确认，以及 D13 至 D15 的功耗、温度适配和以太网硬件设计；单机抓包后冻结 EX201 解析规则。随后按 D12 制定 CAN 与以太网应用协议和双接口控制权规则，验证九阀互斥、运行时序、故障恢复和长期运行，发布可执行的详细设计与验收用例。'
}
for i,t in paragraphs.items(): para(i,t)

insert_after(56, ['系统目标是保证流量计数据及时传输、设定可靠执行，以及阀门控制的实时性和安全性。上位机通过 CAN 或以太网控制和监测 MCU，MCU 通过 RJ45 承载的 RS485 控制和读取六台 MFC，组成统一的控制与状态反馈链路。'])
insert_after(80, ['EX201 使用温度为 5 ℃ 至 50 ℃，精度保证温度为 15 ℃ 至 35 ℃。[S2 印刷页 7] 满足整机 −10 ℃ 至 +40 ℃ 目标需确认相应等级的设备，或通过保温、加热及散热使仪表局部环境符合规格，并验证低温启动、高温运行和计量性能；温控耗电纳入待机预算。'])
insert_after(105, ['上述 43.2 W 是六台 MFC 的保守供电估算，不能作为整机待机功耗结论。待机建议定义为无气体输出、外部阀关闭、MCU 及上行通讯可用；MFC 是否持续预热须明确。以整机输入端测量小于 100 W 为验收目标，统计控制板、MFC、通讯模块、阀驱动和温控等负载；仪表是否包含在测量边界及适用能效标准编号由需求确认。'])
insert_after(158, [
    'CAN 侧采用片内经典 CAN 控制器配合外部收发器，不按 CAN FD 设计。[S5] 波特率、节点地址、标准帧或扩展帧标识符分配、心跳周期和终端配置另行制定。超出单帧载荷的业务参数需定义分段、重组、长度上限与超时，接收完整并校验后才提交业务层。',
    '以太网侧初步建议使用 TCP 承载应用报文；控制器或模块、速率、IP 配置方式及服务端口在硬件和上位机协议评审后确定。应用帧应明确版本、长度、命令、请求号、通道和载荷编码，并限制长度、处理粘包及分包，断连丢弃未完成报文。当前原理图尚需补充完整以太网实现及 MCU 资源分配，不能仅靠软件启用现有 RS485 RJ45。[S4][S5]',
    '双接口可同时读取；控制类请求统一仲裁。首版建议一次仅一个会话取得写控制权，另一接口返回忙或无控制权，停止请求仍可按权限执行。请求用接口来源、会话号和请求号关联，返回已接受、执行中、完成或失败；重复请求返回已有结果。切换控制权应在受控状态下显式完成，重连不自动恢复旧任务。上位机的协调流程不得绕过 MCU 本地互斥。',
    '厂家 Configuration_app 仅用于维护单机验证，接入 MFC 总线前应使 MCU 释放下行总线并禁止启动，避免两个主站同时发送；CAN 和以太网上位机正常运行时均通过 MCU 访问流量计。'
])

editrow(60,'RJ45 与 RS485',['RJ45 与通讯接口','RJ45 为连接器形式。下行 RJ45 承载 RS485；上行独立 RJ45 承载以太网，另设 CAN 接口；三者电气层和用途须区分。'])
editrow(63,'S3',['S3','无标题.png。参考六路气体、九个外部阀位置及手动优先要求；与 S6 冲突的公共阀开闭组合以 S6 为准。'])
addrow(63,['S6','项目补充要求，2026-09-10：上行同时支持 CAN 和以太网，V7/V8 与 V9 互斥，待机功耗小于 100 W，环境 −10 ℃ 至 +40 ℃。'])
editrow(74,'通路控制',['通路控制','统一管理 V1 至 V9；V7 或 V8 开启时禁止 V9，V9 开启时禁止 V7 和 V8。所有本地与远程入口执行同一互斥规则。'])
addrow(74,['上行控制与监测','同时提供 CAN 和以太网接口，读取整机、阀门与六路 MFC 状态；支持上位机协调，执行结果经 MCU 确认后返回。'])
addrow(74,['待机功耗','系统待机功耗必须小于 100 W，按确认的测量边界与待机负载实测。'])
addrow(74,['环境与可靠性','整机在 −10 ℃ 至 +40 ℃ 环境稳定工作；完成仪表温度适配及长期运行验证，验收时长待确认。'])
editrow(74,'恢复管理',['恢复管理','复位后默认关断；重连先核对参数和状态，再等待新的启动请求。'])
editrow(74,'可维护性',['可维护性','驱动、协议、通道和业务分层；硬件极性、阈值及延迟可追溯。'])
editrow(78,'MCU',['MCU','R7FA4M1AB3CFM，RA4M1，64 引脚 LQFP，48 MHz Arm Cortex-M4F；256 KB Code Flash、8 KB Data Flash、32 KB SRAM。[S4][S5]'])
editrow(78,'外部通讯',['上行通讯','CAN 与以太网两种独立接口。CAN 收发器及装配待核对；以太网须补充外接控制器或模块、网口及 MCU 引脚资源。[S4][S6]'])
editrow(78,'MFC 通讯',['MFC 通讯','SCI2：P302/TXD2、P301/RXD2；9600 bps、8N1，六台共用。[S1][S4]'])
editrow(78,'软件工具',['软件工具','建议 e² studio、Arm GCC、Renesas FSP；版本在建工程时锁定。'])
editrow(78,'调度与存储',['调度与存储','中断收发与非阻塞主循环，静态内存；片内 Data Flash 保存低频配置。'])
editrow(88,'通讯服务',['MFC 通讯','单事务调度、帧解析、超时重试与设备地址匹配','mfc_submit / mfc_poll'])
editrow(88,'硬件抽象',['硬件抽象','SCI、CAN、以太网模块、方向控制、GPIO、时基、Flash、看门狗','bsp_uart / bsp_can / bsp_eth'])
addrow(88,['上行接口','CAN 与以太网适配、会话控制权、请求与结果关联、状态上报','host_dispatch / host_snapshot'])
addrow(133,['请求关联','source、session_id、request_id、result、control_owner','上行请求接受、完成、取消或失联时更新'])
for key,val in [('V7 正常输出通路','V7 公共通路'),('V8 排空或测试入口','V8 公共通路'),('V9 公共辅助通路','V9 互斥通路')]:
    setcell(cells(rowmatch(145,key))[0],val)
rowset(nodes[149].findall('w:tr',NS)[0],['组合及条件','V1 至 V6','V7','V8','V9'])
rebuild(149,[
    ['待机或故障关断','全部关闭','关闭','关闭','关闭'],
    ['V7 通路允许组合','按工艺许可','开启','关闭','关闭'],
    ['V8 通路允许组合','按工艺许可','关闭','开启','关闭'],
    ['V7 与 V8 允许组合','按工艺许可','开启','开启','关闭'],
    ['V9 通路允许组合','按工艺许可','关闭','关闭','开启'],
    ['禁止组合','禁止执行','开启','关闭','开启'],
    ['禁止组合','禁止执行','关闭','开启','开启'],
    ['禁止组合','禁止执行','开启','开启','开启']
])
editrow(159,'读取快照',['读取快照','整机状态、六路流量与参数、阀状态、报警、有效性和数据年龄','读取统一缓存；支持 CAN 与以太网，不重复触发下行查询'])
addrow(159,['阀门与通路','请求号、阀掩码或批准的通路模式；返回执行进度或联锁原因','通过阀门管理器执行；输出命令与实际反馈分开返回'])
addrow(159,['控制会话','接口来源、控制权申请/释放、心跳与请求结果查询','单一写控制权；双接口读取；失联不自动切换控制权'])
addrow(168,['阀门互斥冲突','V9 与 V7 或 V8 同时被请求开启','拒绝冲突请求并记录；若输出状态已违反规则，立即撤销相关输出并锁定故障'])
addrow(168,['上行链路或会话失效','CAN bus-off、以太网断连、控制会话心跳超时','上报链路异常；当前远程控制会话按确认的策略停止，拒绝旧请求重放'])
editrow(172,'阀门与通路',['阀门与通路','无危险介质时验证九阀、许可、分组供电及 V7/V8/V9 全部 8 种组合','仅允许表 4.4 所列组合；冲突开阀被拒绝；切换先关后开'])
# The document has no numbered tables; use the actual section locator.
setcell(cells(rowmatch(172,'阀门与通路'))[2],'仅允许第 4.4 节所列组合；冲突开阀被拒绝；切换先关后开')
addrow(172,['CAN 与以太网','分别验证状态读取、流量设定、阀门控制和结果查询；注入错误帧、分段及断连','两接口结果一致；未完成帧不执行；成功仅在实际事务完成后返回'])
addrow(172,['双接口并发','两接口同时请求相斥开阀、重复设定、停止、控制权切换和超时恢复','单一写控制权；全部入口保持互斥；停止后迟到请求不重新开阀'])
addrow(172,['待机功耗','按确认边界测整机输入功率；记录 MFC 预热、网口及温控工作状态','在规定待机及环境条件下小于 100 W；保留负载和测量记录'])
addrow(172,['环境与长期可靠性','−10 ℃、+40 ℃ 下验证启动、通讯、阀动作和持续运行；记录仪表局部温度','整机功能稳定；MFC 局部温度及计量条件符合手册，时长按最终验收计划'])
editrow(176,'D07',['D07','J10 与 EX201 的 DB9 引脚差异；下行 RS485 RJ45 线序、终端、参考地及屏蔽；上行 CAN 实际装配和连接器','冻结独立上下行线束；禁止以太网与 RS485 网口混接'])
editrow(176,'D11',['D11','在已确定的 V7/V8 与 V9 互斥规则内，确认正常输出、排空、校准对应组合、V9 去向、压差及许可气体组合','冻结各模式和启动、切换时序；未经批准模式不启用'])
editrow(176,'D12',['D12','CAN 波特率、ID 与分段；以太网地址、应用协议与端口；双接口控制权、心跳及断连策略、响应时限和日志需求','冻结上位机接口规范与实时性验收条件'])
addrow(176,['D13','小于 100 W 的整机测量边界、待机定义、MFC 预热及温控负载；适用能效标准编号','建立完整功耗预算和可复现的测量条件'])
addrow(176,['D14','−10 ℃ 至 +40 ℃ 系统目标与 EX201 使用 5 ℃ 至 50 ℃、精度保证 15 ℃ 至 35 ℃ 的适配；温控和耐久时长','确认仪表型号或局部温控方案，冻结环境与计量验收'])
addrow(176,['D15','外接以太网控制器或模块、MAC/PHY、RJ45 磁性器件、隔离保护、供电、MCU 引脚和驱动资源','补充硬件并核算 32 KB SRAM 与 256 KB Flash；形成以太网 BSP'])

# Fresh technical diagram, matched to the template's black-and-white style.
im = Image.new('RGB',(1800,820),'white'); dr = ImageDraw.Draw(im)
font = ImageFont.truetype(r'C:\Windows\Fonts\simsun.ttc',31)
small = ImageFont.truetype(r'C:\Windows\Fonts\simsun.ttc',27)
def box(x,y,w,h,txt):
    dr.rectangle((x,y,x+w,y+h),outline='black',width=3)
    ls=txt.split('\n')
    for j,line in enumerate(ls):
        bb=dr.textbbox((0,0),line,font=font)
        dr.text((x+(w-(bb[2]-bb[0]))/2,y+(h-len(ls)*38)/2+j*38),line,font=font,fill='black')
def arrow(points, both=False):
    dr.line(points,fill='black',width=3)
    def head(p,q):
        a=math.atan2(q[1]-p[1],q[0]-p[0])
        dr.polygon([q]+[(q[0]-15*math.cos(a+d),q[1]-15*math.sin(a+d)) for d in [-.5,.5]],fill='black')
    head(points[-2],points[-1])
    if both: head(points[1],points[0])
box(30,270,255,170,'上位机\n控制与监测\n检测与过程协调')
box(370,90,370,140,'CAN 上行接口\n片内 CAN＋收发器\n连接器及装配核对')
box(370,460,370,180,'以太网上行接口\n外接控制器或模块\n独立 RJ45 网口\n硬件待补充')
box(865,245,350,250,'R7FA4M1AB3CFM\n统一命令与控制权\n六路通道对象\n本地阀门联锁\n非阻塞通讯调度')
box(1340,85,430,170,'SCI2＋隔离 RS485\nJ9/J15 下行 RJ45\n一条总线接六台 MFC\n地址 001 至 006')
box(1340,430,430,180,'GPIO＋隔离驱动\nV1 至 V6 支路阀\nV7/V8 与 V9 互斥\n阀电源按组管理')
box(865,640,350,100,'急停与手动许可\n信号接入待确认')
arrow([(285,315),(325,315),(325,160),(370,160)],True)
arrow([(285,395),(325,395),(325,550),(370,550)],True)
arrow([(740,160),(795,160),(795,315),(865,315)],True)
arrow([(740,550),(795,550),(795,435),(865,435)],True)
arrow([(1215,315),(1270,315),(1270,170),(1340,170)],True)
arrow([(1215,435),(1270,435),(1270,520),(1340,520)])
arrow([(1040,640),(1040,495)])
dr.text((30,775),'注：上行以太网 RJ45 与下行 RS485 RJ45 为独立端口；CAN 与以太网共用业务层。',font=small,fill='black')
imgpath=TMP/'hardware-v02.png';im.save(imgpath)
parts['word/media/image1.png']=imgpath.read_bytes()
for e in nodes[83].findall('.//wp:extent',NS): e.set('cy',str(round(int(e.get('cx'))*820/1800)))
for e in nodes[83].findall('.//a:xfrm/a:ext',NS): e.set('cy',str(round(int(e.get('cx'))*820/1800)))
for e in nodes[83].findall('.//wp:docPr',NS):
    e.set('descr','上位机经独立 CAN 和以太网接口连接 MCU；MCU 经一条 RS485 总线管理六台 MFC，并执行九阀互斥。以太网硬件待补充。')

# Adjust only local spacing to avoid sparse overflow pages, retaining source fonts,
# numbering, body line spacing, section geometry and table grid appearance.
def ensure(parent, name):
    e=parent.find('w:'+name,NS)
    if e is None: e=ET.SubElement(parent,tag(name))
    return e
for p in body.findall('w:p',NS):
    pr=p.find('w:pPr',NS)
    if pr is not None:
        st=pr.find('w:pStyle',NS)
        if st is not None and st.get(tag('val')) in ('3','4'):
            sp=ensure(pr,'spacing');sp.set(tag('before'),'120');sp.set(tag('after'),'120')
for i in [70,74,78,88,145,149,176]:
    pr=nodes[i].find('w:tblPr',NS)
    mar=ensure(pr,'tblCellMar')
    for side in ['top','bottom']:
        e=ensure(mar,side);e.set(tag('w'),'40');e.set(tag('type'),'dxa')
# Keep the complete three-valve truth table together for comparison.
trs=nodes[149].findall('w:tr',NS)
for row in trs[:-1]:
    for p in row.findall('.//w:p',NS):
        pr=ensure(p,'pPr');ensure(pr,'keepNext').set(tag('val'),'1')

alltext=textof(body)
for forbidden in ['再建立 V7/V9 通路','V7 与 V9 按资料要求开启','正常需 V7、V9 开','上行 RS485/CAN 协议','本设计的 RJ45 直接用于 RS485','若采用 Modbus RTU 作为上行协议']:
    assert forbidden not in alltext, forbidden
for required in ['CAN 和以太网两种','小于 100 W','−10 ℃ 至 +40 ℃','NOT [V9 AND (V7 OR V8)]','D15']:
    assert required in alltext, required
# Preserve the source format; only refresh requested contents, figure, metadata and fields.
parts['word/document.xml']=ET.tostring(root,xml_declaration=True,encoding='UTF-8',standalone=True)
settings=ET.fromstring(parts['word/settings.xml'])
uf=settings.find('w:updateFields',NS)
if uf is None: uf=ET.SubElement(settings,tag('updateFields'))
uf.set(tag('val'),'true')
parts['word/settings.xml']=ET.tostring(settings,xml_declaration=True,encoding='UTF-8',standalone=True)
if 'docProps/core.xml' in parts:
    core=ET.fromstring(parts['docProps/core.xml'])
    for e in core:
        if ET.QName(e).localname=='version': e.text='0.2'
    parts['docProps/core.xml']=ET.tostring(core,xml_declaration=True,encoding='UTF-8',standalone=True)
with ZipFile(OUT,'w',ZIP_DEFLATED) as z:
    for name,data in parts.items(): z.writestr(name,data)
(TMP/'revised-text.txt').write_text('\n'.join(textof(e) for e in body),encoding='utf-8')
(TMP/'authoring-qa.json').write_text(json.dumps({'source_sha256':sha256(SOURCE.read_bytes()).hexdigest(),'output':str(OUT),'revised_paragraphs':len(paragraphs),'tables':len(body.findall('w:tbl',NS)),'valve_truth_table':[{ 'V7':a,'V8':b,'V9':c,'allowed':not(c and(a or b))} for a in (0,1) for b in (0,1) for c in (0,1)]},ensure_ascii=False,indent=2),encoding='utf-8')
print(OUT)
