/*
 * Created on: 2026年9月20日
 * Author: CI
 */
#include "H_MfcCan.h"
#include "MfcTask.h"
#include <string.h>

#define H_MFCCAN_SPI_WAIT_US (1000U) // 单条SPI指令有界等待，中断始终开启
#define H_MFCCAN_CNF1 (0x01U) // BRP=1，TQ=0.5us
#define H_MFCCAN_CNF2 (0x98U) // BTLMODE=1，单采样，PS1=4TQ，PROP=1TQ
#define H_MFCCAN_CNF3 (0x01U) // PS2=2TQ；总8TQ=4us，采样点75%

/*
 * 说明：SPI回调只更新对应上下文的完成状态
 * 输入：p_args FSP参数
 * 输出：无
 */
void H_MFC_CAN_SpiCallback(spi_callback_args_t *p_args)
{
    H_MfcCan_Context *p_context = NULL; // 回调绑定的长期状态
    if (NULL == p_args || NULL == p_args->p_context) { return; }
    p_context = (H_MfcCan_Context *) p_args->p_context;
    if (SPI_EVENT_TRANSFER_COMPLETE != p_args->event) { p_context->spi_error = 1U; }
    p_context->spi_done = 1U;
}

/*
 * 说明：保持一次硬件SSL片选传输整条MCP命令，等待时间有界
 * 输入：p_context 长期缓冲，length 本次字节数
 * 输出：uint32_t 非0成功，超时或错误时关闭SPI并锁存故障
 */
static uint32_t H_MfcCan_Transfer(H_MfcCan_Context *p_context, uint32_t length)
{
    uint32_t waited = 0U; // 已等待的微秒轮数，不含被高优先级任务抢占时间
    if (__get_IPSR() != 0U || __get_PRIMASK() != 0U || __get_BASEPRI() != 0U)
    {
        p_context->fault = 1U;
        return 0U; // SPI完成依赖中断，不能在屏蔽中断时等待。
    }
    p_context->spi_done = 0U;
    p_context->spi_error = 0U;
    if (FSP_SUCCESS == g_mfc_spi.p_api->writeRead(g_mfc_spi.p_ctrl,
            p_context->spi_tx, p_context->spi_rx, length, SPI_BIT_WIDTH_8_BITS))
    {
        while (!p_context->spi_done && waited < H_MFCCAN_SPI_WAIT_US)
        {
            R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MICROSECONDS);
            waited++;
        }
        if (p_context->spi_done && !p_context->spi_error) { return 1U; }
    }
    p_context->fault = 1U;
    if (FSP_SUCCESS == g_mfc_spi.p_api->close(g_mfc_spi.p_ctrl)) { p_context->spi_open = 0U; }
    return 0U;
}

/*
 * 说明：连续写MCP2515寄存器
 * 输入：p_context 状态，address 起点，p_data 数据，length 数据长度不超过13
 * 输出：uint32_t 非0成功
 */
static uint32_t H_MfcCan_Write(H_MfcCan_Context *p_context, uint8_t address,
                               const uint8_t *p_data, uint32_t length)
{
    p_context->spi_tx[0] = 0x02U;
    p_context->spi_tx[1] = address;
    memcpy(&p_context->spi_tx[2], p_data, length);
    return H_MfcCan_Transfer(p_context, length + 2U);
}

/*
 * 说明：读取一个寄存器；失败时不更新输出
 * 输入：p_context 状态，address 地址，p_value 输出值
 * 输出：uint32_t 非0成功
 */
static uint32_t H_MfcCan_Read(H_MfcCan_Context *p_context, uint8_t address, uint8_t *p_value)
{
    p_context->spi_tx[0] = 0x03U;
    p_context->spi_tx[1] = address;
    p_context->spi_tx[2] = 0U;
    if (!H_MfcCan_Transfer(p_context, 3U)) { return 0U; }
    *p_value = p_context->spi_rx[2];
    return 1U;
}

/*
 * 说明：原子修改支持BIT MODIFY的状态寄存器，避免覆盖新到的中断标志
 * 输入：p_context 状态，address 地址，mask 位掩码，value 新位值
 * 输出：uint32_t 非0成功
 */
static uint32_t H_MfcCan_Modify(H_MfcCan_Context *p_context, uint8_t address,
                                uint8_t mask, uint8_t value)
{
    p_context->spi_tx[0] = 0x05U;
    p_context->spi_tx[1] = address;
    p_context->spi_tx[2] = mask;
    p_context->spi_tx[3] = value;
    return H_MfcCan_Transfer(p_context, 4U);
}

/*
 * 说明：把29位ID拆成MCP2515的SIDH/SIDL/EID8/EID0
 * 输入：id 标识符，p_bytes 四字节输出，extended EXIDE位；掩码寄存器该位必须为0
 * 输出：无
 */
static void H_MfcCan_PackId(uint32_t id, uint8_t *p_bytes, uint8_t extended)
{
    p_bytes[0] = (uint8_t) (id >> 21U);
    p_bytes[1] = (uint8_t) (((id >> 13U) & 0xE0U) | ((id >> 16U) & 0x03U) | extended);
    p_bytes[2] = (uint8_t) (id >> 8U);
    p_bytes[3] = (uint8_t) id;
}

/*
 * 说明：打开SPI并安装状态回调；没有绑定业务全局变量
 * 输入：p_context 状态
 * 输出：uint32_t 非0成功
 */
static uint32_t H_MfcCan_Open(H_MfcCan_Context *p_context)
{
    if (p_context->spi_open) { return 1U; }
    if (FSP_SUCCESS != g_mfc_spi.p_api->open(g_mfc_spi.p_ctrl, g_mfc_spi.p_cfg)) { return 0U; }
    p_context->spi_open = 1U;
    if (FSP_SUCCESS != g_mfc_spi.p_api->callbackSet(g_mfc_spi.p_ctrl, H_MFC_CAN_SpiCallback, p_context, NULL))
    {
        if (FSP_SUCCESS == g_mfc_spi.p_api->close(g_mfc_spi.p_ctrl)) { p_context->spi_open = 0U; }
        return 0U;
    }
    return 1U;
}

/*
 * 说明：软件复位后验证配置模式，不能把SPI返回全0或全FF当作芯片存在
 * 输入：p_context 状态
 * 输出：uint32_t 非0成功
 */
static uint32_t H_MfcCan_Reset(H_MfcCan_Context *p_context)
{
    uint8_t value = 0U; // 复位后的CANSTAT
    p_context->spi_tx[0] = 0xC0U;
    if (!H_MfcCan_Transfer(p_context, 1U)) { return 0U; }
    R_BSP_SoftwareDelay(100U, BSP_DELAY_UNITS_MICROSECONDS);
    return H_MfcCan_Read(p_context, 0x0EU, &value) && (value & 0xE0U) == 0x80U;
}

/*
 * 说明：进入250k正常模式，六个过滤器采用相同条件，启用RXB0向RXB1溢出
 * 输入：p_context 状态，filter_id/filter_mask 扩展ID条件
 * 输出：H_MfcCan_Result 结果
 */
H_MfcCan_Result H_MfcCan_Initialize(H_MfcCan_Context *p_context, uint32_t filter_id, uint32_t filter_mask)
{
    static const uint8_t s_filters[6] = {0x00U, 0x04U, 0x08U, 0x10U, 0x14U, 0x18U}; // 过滤器地址
    static const uint8_t s_timing[3] = {H_MFCCAN_CNF3, H_MFCCAN_CNF2, H_MFCCAN_CNF1}; // 连续寄存器顺序
    uint8_t bytes[4] = {0}; // ID寄存器数据
    uint8_t value = 0U; // 单寄存器值
    uint32_t index = 0U; // 过滤器索引
    if (NULL == p_context || filter_id > 0x1FFFFFFFUL || filter_mask > 0x1FFFFFFFUL) { return H_MFCCAN_ERROR; }
    if (p_context->initialized && !p_context->fault) { return H_MFCCAN_OK; }
    p_context->fault = 1U;
    p_context->initialized = 0U;
    if (!H_MfcCan_Open(p_context) || !H_MfcCan_Reset(p_context)) { return H_MFCCAN_ERROR; }
    // MFC_EN2同时影响485方向和CAN待机，只能在旧UART事务已停止时切换。
    if (FSP_SUCCESS != g_ioport.p_api->pinWrite(g_ioport.p_ctrl, MFC_EN2, BSP_IO_LEVEL_LOW)) { return H_MFCCAN_ERROR; }
    if (!H_MfcCan_Write(p_context, 0x28U, s_timing, 3U)) { return H_MFCCAN_ERROR; }
    H_MfcCan_PackId(filter_mask, bytes, 0U);
    if (!H_MfcCan_Write(p_context, 0x20U, bytes, 4U) || !H_MfcCan_Write(p_context, 0x24U, bytes, 4U)) { return H_MFCCAN_ERROR; }
    H_MfcCan_PackId(filter_id, bytes, 0x08U);
    for (index = 0U; index < 6U; index++)
    {
        if (!H_MfcCan_Write(p_context, s_filters[index], bytes, 4U)) { return H_MFCCAN_ERROR; }
    }
    value = 0x04U; // BUKT=1，RXM=00，按过滤器接收，RXM=01/10是保留值。
    if (!H_MfcCan_Write(p_context, 0x60U, &value, 1U)) { return H_MFCCAN_ERROR; }
    value = 0U;
    if (!H_MfcCan_Write(p_context, 0x70U, &value, 1U)) { return H_MFCCAN_ERROR; }
    value = 0x08U; // 正常模式+One-Shot；失败或仲裁丢失不自动重发控制命令。
    if (!H_MfcCan_Write(p_context, 0x0FU, &value, 1U)) { return H_MFCCAN_ERROR; }
    R_BSP_SoftwareDelay(100U, BSP_DELAY_UNITS_MICROSECONDS);
    if (!H_MfcCan_Read(p_context, 0x0EU, &value) || (value & 0xE0U) != 0U) { return H_MFCCAN_ERROR; }
    p_context->transmit_state = 0U;
    p_context->receive_count = 0U;
    p_context->read_index = 0U;
    p_context->fault = 0U;
    p_context->initialized = 1U;
    return H_MFCCAN_OK;
}

/*
 * 说明：读取并释放一个硬件接收缓冲，完整校验帧类型和长度
 * 输入：p_context 状态，buffer 接收缓冲索引0或1
 * 输出：uint32_t 非0成功，环形缓冲溢出视为通信错误
 */
static uint32_t H_MfcCan_ReadBuffer(H_MfcCan_Context *p_context, uint32_t buffer)
{
    H_MfcCan_Frame *p_frame = NULL; // 本任务接收槽
    uint32_t index = 0U; // 接收槽索引
    uint8_t control = 0U; // RXBnCTRL包含标准/扩展帧通用RTR标志
    if (!H_MfcCan_Read(p_context, (uint8_t) (0x60U + buffer * 0x10U), &control)) { return 0U; }
    memset(p_context->spi_tx, 0, sizeof(p_context->spi_tx));
    p_context->spi_tx[0] = (uint8_t) (0x90U + buffer * 4U);
    if (!H_MfcCan_Transfer(p_context, 14U)) { return 0U; } // CS释放时自动清除对应RXnIF。
    if ((p_context->spi_rx[2] & 0x08U) == 0U || (control & 0x08U) != 0U ||
        (p_context->spi_rx[5] & 0x0FU) != 8U)
    {
        p_context->rejected_frames++;
        return 1U;
    }
    if (p_context->receive_count >= H_MFCCAN_RX_CAPACITY) { return 0U; }
    index = (p_context->read_index + p_context->receive_count) % H_MFCCAN_RX_CAPACITY;
    p_frame = &p_context->receive[index];
    p_frame->id = ((uint32_t) p_context->spi_rx[1] << 21U) |
                 ((uint32_t) (p_context->spi_rx[2] & 0xE0U) << 13U) |
                 ((uint32_t) (p_context->spi_rx[2] & 3U) << 16U) |
                 ((uint32_t) p_context->spi_rx[3] << 8U) | p_context->spi_rx[4];
    memcpy(p_frame->data, &p_context->spi_rx[6], 8U);
    p_context->receive_count++;
    return 1U;
}

/*
 * 说明：先处理接收和错误，再检查发送；一次最多搬运两个接收帧
 * 输入：p_context 状态，now 当前节拍
 * 输出：无
 */
void H_MfcCan_Process(H_MfcCan_Context *p_context, TickType_t now)
{
    uint8_t flags = 0U, error = 0U, control = 0U; // CANINTF、EFLG、TXB0CTRL
    uint8_t bytes[13] = {0}; // TXB0的标识符、DLC和数据
    if (NULL == p_context || !p_context->initialized || p_context->fault) { return; }
    if (!H_MfcCan_Read(p_context, 0x2DU, &error) || (error & 0xE0U) != 0U ||
        !H_MfcCan_Read(p_context, 0x2CU, &flags)) { p_context->fault = 1U; return; }
    if (((flags & 1U) != 0U && !H_MfcCan_ReadBuffer(p_context, 0U)) ||
        ((flags & 2U) != 0U && !H_MfcCan_ReadBuffer(p_context, 1U))) { p_context->fault = 1U; return; }
    if (p_context->transmit_state == 2U)
    {
        if (!H_MfcCan_Read(p_context, 0x30U, &control)) { return; }
        if ((control & 0x08U) != 0U)
        {
            if ((TickType_t) (now - p_context->transmit_tick) >= H_MFCCAN_TX_TIMEOUT_MS) { p_context->fault = 1U; }
            return;
        }
        if (!H_MfcCan_Read(p_context, 0x2CU, &flags) ||
            (control & 0x70U) != 0U || (flags & 0x04U) == 0U) { p_context->fault = 1U; return; }
        p_context->transmit_state = 0U;
    }
    if (p_context->transmit_state != 1U) { return; }
    if (!H_MfcCan_Modify(p_context, 0x2CU, 0x04U, 0U)) { return; }
    H_MfcCan_PackId(p_context->transmit.id, bytes, 0x08U);
    bytes[4] = 8U;
    memcpy(&bytes[5], p_context->transmit.data, 8U);
    if (!H_MfcCan_Write(p_context, 0x31U, bytes, 13U)) { return; }
    p_context->spi_tx[0] = 0x81U; // RTS，仅使用TXB0。
    if (!H_MfcCan_Transfer(p_context, 1U)) { return; }
    p_context->transmit_tick = now;
    p_context->transmit_state = 2U;
}

/*
 * 说明：复制待发帧，不等待SPI或CAN，因此可用于最终授权检查后的启动步骤
 * 输入：p_context 状态，p_frame 待发帧
 * 输出：H_MfcCan_Result 结果
 */
H_MfcCan_Result H_MfcCan_Send(H_MfcCan_Context *p_context, const H_MfcCan_Frame *p_frame)
{
    if (NULL == p_context || NULL == p_frame || p_frame->id > 0x1FFFFFFFUL ||
        !p_context->initialized || p_context->fault) { return H_MFCCAN_ERROR; }
    if (p_context->transmit_state != 0U) { return H_MFCCAN_BUSY; }
    p_context->transmit = *p_frame;
    p_context->transmit_state = 1U;
    return H_MFCCAN_OK;
}

/*
 * 说明：取得发送完成状态，CAN ACK不代表对端业务执行成功
 * 输入：p_context 状态
 * 输出：H_MfcCan_Result 结果
 */
H_MfcCan_Result H_MfcCan_GetTransmitState(const H_MfcCan_Context *p_context)
{
    if (NULL == p_context || !p_context->initialized || p_context->fault) { return H_MFCCAN_ERROR; }
    return p_context->transmit_state != 0U ? H_MFCCAN_BUSY : H_MFCCAN_OK;
}

/*
 * 说明：从内部缓冲取帧，异常时不再提供旧数据
 * 输入：p_context 状态，p_frame 输出帧
 * 输出：H_MfcCan_Result 结果
 */
H_MfcCan_Result H_MfcCan_Receive(H_MfcCan_Context *p_context, H_MfcCan_Frame *p_frame)
{
    if (NULL == p_context || NULL == p_frame || !p_context->initialized || p_context->fault) { return H_MFCCAN_ERROR; }
    if (p_context->receive_count == 0U) { return H_MFCCAN_EMPTY; }
    *p_frame = p_context->receive[p_context->read_index];
    p_context->read_index = (p_context->read_index + 1U) % H_MFCCAN_RX_CAPACITY;
    p_context->receive_count--;
    return H_MFCCAN_OK;
}

/*
 * 说明：确认芯片进入配置模式后关闭SPI，清掉旧发送及接收缓存
 * 输入：p_context 状态
 * 输出：H_MfcCan_Result 停止结果
 */
H_MfcCan_Result H_MfcCan_Stop(H_MfcCan_Context *p_context)
{
    if (NULL == p_context) { return H_MFCCAN_ERROR; }
    if (!p_context->initialized && p_context->transmit_state == 0U)
    {
        // 初始化未完成且从未提交RTS；芯片缺席不能阻止继续探测RS485。
        if (p_context->spi_open && FSP_SUCCESS != g_mfc_spi.p_api->close(g_mfc_spi.p_ctrl)) { return H_MFCCAN_ERROR; }
        p_context->spi_open = 0U;
        p_context->fault = 0U;
        return H_MFCCAN_OK;
    }
    p_context->fault = 1U;
    if (!H_MfcCan_Open(p_context) || !H_MfcCan_Reset(p_context)) { return H_MFCCAN_ERROR; }
    if (FSP_SUCCESS != g_mfc_spi.p_api->close(g_mfc_spi.p_ctrl)) { return H_MFCCAN_ERROR; }
    p_context->spi_open = 0U;
    p_context->initialized = 0U;
    p_context->transmit_state = 0U;
    p_context->receive_count = 0U;
    p_context->read_index = 0U;
    p_context->fault = 0U;
    return H_MFCCAN_OK;
}
