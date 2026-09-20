# 主板软件运行流程：FreeRTOS

适用版本：V1.8.0.260920。更新日期：2026-09-20。本文描述当前代码；历史Word、XMind和Visio方案以README注明的版本为准。

三个任务保持不变：HostCanStack负责上位机CAN0，MfcTask负责两个下行候选后端和六个通道，ControlTask负责命令协调及九阀控制。任务间全部使用现有八个静态FreeRTOS队列。下行CAN为250 kbit/s、FLOW节点1～6，使用本项目自定义CAN_USER参数表1；RS485仍使用EX-201S原厂ASCII协议。

## 1. 上电与三个任务

```mermaid
flowchart TD
    BOOT[复位 / FSP初始化] --> RTOS[启动FreeRTOS调度器]
    RTOS --> HOST[HostCanStack 优先级2<br/>初始化上位机CAN0和CAN_USER]
    RTOS --> MFC[MfcTask 优先级3<br/>校验六路地址，进入只读探测]
    RTOS --> CONTROL[ControlTask 优先级4<br/>初始化九阀并关闭输出]
    HOST --> HLOOP[收帧 / 查本地快照 / 下发命令<br/>处理执行结果 / 发送回复]
    MFC --> MLOOP[识别并锁定下行链路<br/>六路初始化、轮询、单次写入读回]
    CONTROL --> CLOOP[命令出队与授权检查<br/>九阀联锁和200ms吸合保持]
    HLOOP --> HD[延时1ms] --> HLOOP
    MLOOP --> MD[延时1ms] --> MLOOP
    CLOOP --> CD[延时1ms] --> CLOOP
```

任务栈各1024字节，实际峰值和调度延迟待实板测量。驱动由所属任务打开，两个下行后端按探测阶段依次准备；不在调度器启动前等待六台设备上线。1ms是任务延时间隔，不是完整六路扫描时间。

## 2. 任务间的命令和数据

```mermaid
flowchart LR
    PC[上位机 CAN_USER] <--> HOST[HostCanStack / CAN0]
    HOST -->|host_command_queue| CTRL[ControlTask]
    CTRL -->|host_result_queue| HOST
    CTRL -->|command_queue| MFC[MfcTask]
    MFC -->|result_queue| CTRL
    MFC -->|telemetry_queue 六路完整快照| HOST
    CTRL -->|valve_state_queue 九阀快照| HOST
    HOST -->|control_state_queue 请求授权| CTRL
    HOST -->|mfc_state_queue 请求授权| MFC
    MFC --> LINK{已锁定的后端}
    LINK -->|RS485| EX[A_EX201 / F_EX201 / H_EX201<br/>SCI2与仪器原厂协议]
    LINK -->|CAN| CAN[A_MfcCan / F_MfcCan / H_MfcCan<br/>SPI1 / MCP2515 / CAN_USER]
```

上位机查询读取HostCanStack自己的快照；MFC自主循环采集，不等待上位机发起每笔测量。上位机写请求由Control转成通道和业务操作，原始CAN0帧不会直接转发到MCP2515。每次只允许一笔未完成写命令，快照队列覆盖最新完整状态。

## 3. MfcTask内部识别

```mermaid
stateDiagram-v2
    [*] --> 准备探测
    准备探测 --> RS485首读: 确认旧CAN及UART停止
    RS485首读 --> RS485复核: RMFS完整应答正确
    RS485复核 --> 锁定RS485: 同节点RDPP正确
    RS485首读 --> CAN首读: 失败，静默后准备CAN
    RS485复核 --> CAN首读: 失败，静默后准备CAN
    CAN首读 --> CAN复核: 0x0100标识正确
    CAN复核 --> 锁定CAN: 同节点0x0108版本为1
    CAN首读 --> 下一节点: 失败
    CAN复核 --> 下一节点: 失败
    下一节点 --> 准备探测: 静默250ms，节点1至6循环
    锁定RS485 --> 六路业务: 每路九项初始化
    锁定CAN --> 六路业务: 每节点标识与版本通过，再九项初始化
    六路业务 --> 六路业务: 单路失败只使本路退避
    六路业务 --> 清旧状态: 整链路10秒无有效响应或恢复失败
    清旧状态 --> 准备探测: 清缓存，停止旧事务，不重放旧写入
```

由`A_MfcLink_Process`管理探测。同一节点的两个不同只读应答均正确才锁定；一个节点足够确认链路，之后仍独立管理六路。锁定后不再扫描另一后端，不按单个报文反复切换。

`MFC_EN2`同时影响RS485方向和CAN待机，两后端不能随意并行改变该脚。停止旧事务失败时保持故障并重试；固件不读取外部跳线，不更改`MFC_RES2`终端策略。CAN采用本项目协议，需要兼容设备或转换模块，不能直接假定EX-201S支持CAN。

## 4. 两种后端的单笔读取

```mermaid
flowchart TD
    SELECT[A_MFC_SelectRead<br/>轮流选择六路到期参数] --> LINK{selected_link}
    LINK -->|RS485| RTX[A_EX201_StartRequest<br/>编码EX201 ASCII，启动SCI2发送]
    RTX --> RTXC[UART TX_COMPLETE<br/>切回接收]
    RTXC --> RRX[接收到完整响应<br/>检查地址、命令、校验、OK/NG]
    LINK -->|CAN| CTX[A_MfcCan_Start<br/>CAN_USER编码并复制到待发槽]
    CTX --> CSPI[H_MfcCan_Process<br/>SPI写TXB0并RTS]
    CSPI --> CRX[轮询CANINTF / EFLG / RXB0 / RXB1<br/>检查ID、DLC、校验和关联字段]
    RRX --> VALUE[检查参数类型、范围、单位和小数位]
    CRX --> VALUE
    VALUE --> CACHE[仅更新当前通道缓存与有效位]
    CACHE --> QUEUE[telemetry_queue<br/>上位机任务取得最新六路快照]
```

RS485发送期限100ms，应答期限从发送完成后起算500ms；CAN从提交请求起200ms，MCP2515单帧发送50ms。SPI单条命令有界等待完成，中断保持开启，业务应答通过状态机等待。CAN错误后静默250ms，RS485错误后50ms；实际响应上界与迟到帧仍需联调确认。

## 5. 单次流量写入

```mermaid
flowchart TD
    Q[command_queue出队] --> CHECK[核对授权、期限、链路锁定和通道就绪]
    CHECK --> SOURCE[读数字来源<br/>RS485 RFSM / CAN 0x0104]
    SOURCE --> VALUE[再次核对量程和分辨率]
    VALUE --> WRITE[只提交一次写入<br/>RS485 WSFD / CAN 0x0200]
    WRITE --> ACK[等待设备业务回复<br/>EX201 OK / CAN 0x06值0]
    ACK --> READ[读数字设定<br/>RS485 RSFD / CAN 0x0001]
    READ --> COMPARE{读回尾数一致}
    COMPARE -->|是| OK[更新确认值与MCU目标<br/>结果队列返回成功]
    COMPARE -->|否| ERROR[返回读回不一致]
```

完整请求期限3000ms，包含排队。写入、应答或读回失败不自动重发；应答丢失时设定可能已生效。CAN待发槽在下一轮授权检查通过后才推进硬件；取消时尚未发出的帧丢弃，已发出的动作无法撤回。

## 6. 九阀与当前实现边界

V1～V6独立，V7与V9同步、共同与V8互斥。CAN通过V7控制这对阀，V9地址只读。新开阀由ControlTask控制12V吸合200ms后约5V保持，三组共享电压；规则和GPIO实现沿用已有版本。

本版探测期间禁止流量写入，但未加入下行失联后自动关闭所有外部阀或将仪器归零的完整气路过程。重新探测不能被解释为已确认气路安全停机。详见[九路电磁阀执行说明](九路电磁阀执行说明.md)。

实际函数索引见[主板运行流程与收发函数说明](主板运行流程与收发函数说明.md)第14节；新参数表、错误码、报文样例和响应约束见[MFC下行CAN_USER协议与自动识别](MFC下行CAN_USER协议与自动识别.md)。模拟测试和编译通过不等于实板通信、使能电平、阀门或气路验证完成。
