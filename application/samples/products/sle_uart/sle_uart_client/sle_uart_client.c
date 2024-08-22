/**
 * Copyright (c) @CompanyNameMagicTag 2023-2023. All rights reserved. \n
 *
 * Description: SLE uart sample of client. \n
 * Author: @CompanyNameTag \n
 * History: \n
 * 2023-04-03, Create file. \n
 */
#include "common_def.h"
#include "soc_osal.h"
#include "securec.h"
#include "product.h"
#include "bts_le_gap.h"
#include "bts_device_manager.h"
#include "sle_device_manager.h"
#include "sle_device_discovery.h"
#include "sle_connection_manager.h"
#include "sle_uart_client.h"
#ifdef CONFIG_SAMPLE_SUPPORT_LOW_LATENCY_TYPE
#include "sle_low_latency.h"
#endif
#define SLE_MTU_SIZE_DEFAULT            520
#define SLE_SEEK_INTERVAL_DEFAULT       100
#define SLE_SEEK_WINDOW_DEFAULT         100
#define UUID_16BIT_LEN                  2
#define UUID_128BIT_LEN                 16
#define SLE_UART_TASK_DELAY_MS          1000
#define SLE_UART_WAIT_SLE_CORE_READY_MS 5000
#define SLE_UART_RECV_CNT               1000
#define SLE_UART_LOW_LATENCY_2K         2000
#ifndef SLE_UART_SERVER_NAME
#define SLE_UART_SERVER_NAME            "sle_uart_server"
#endif
#define SLE_UART_CLIENT_LOG             "[sle uart client]"

static ssapc_find_service_result_t g_sle_uart_find_service_result = { 0 };
static sle_dev_manager_callbacks_t g_sle_dev_mgr_cbk = { 0 };
static sle_announce_seek_callbacks_t g_sle_uart_seek_cbk = { 0 };
static sle_connection_callbacks_t g_sle_uart_connect_cbk = { 0 };
static ssapc_callbacks_t g_sle_uart_ssapc_cbk = { 0 };
static sle_addr_t g_sle_uart_remote_addr = { 0 };
ssapc_write_param_t g_sle_uart_send_param = { 0 };
uint16_t g_sle_uart_conn_id = 0;

uint16_t get_g_sle_uart_conn_id(void)
{
    return g_sle_uart_conn_id;
}

ssapc_write_param_t *get_g_sle_uart_send_param(void)
{
    return &g_sle_uart_send_param;
}

/**
 * @brief		开SCNA
 * @param[in]	none
 * @return      none
 */
void sle_uart_start_scan(void)
{
    /* 配置scan参数 */
    sle_seek_param_t param = { 0 };
    param.own_addr_type = 0;
    param.filter_duplicates = 0;
    param.seek_filter_policy = 0;
    param.seek_phys = 1;
    param.seek_type[0] = 1;
    param.seek_interval[0] = SLE_SEEK_INTERVAL_DEFAULT;
    param.seek_window[0] = SLE_SEEK_WINDOW_DEFAULT;
    sle_set_seek_param(&param);
    /* 开始SCAN */
    sle_start_seek();
}

/**
 * @brief		SEL上电回调
 * @param[in]	status：状态，具体含义未知
 * @return      none
 */
static void sle_uart_client_sample_sle_power_on_cbk(uint8_t status)
{
    osal_printk("sle power on: %d.\r\n", status);
    enable_sle();
}

/**
 * @brief		SEL使能回调
 * @param[in]	status：状态，具体含义未知
 * @return      none
 */
static void sle_uart_client_sample_sle_enable_cbk(uint8_t status)
{
    osal_printk("sle enable: %d.\r\n", status);
    /* client初始化 */
    sle_uart_client_init(sle_uart_notification_cb, sle_uart_indication_cb);
    /* 开启SCAN */
    sle_uart_start_scan();
}

static void sle_uart_client_sample_seek_enable_cbk(errcode_t status)
{
    if (status != 0)
    {
        osal_printk("%s sle_uart_client_sample_seek_enable_cbk,status error\r\n", SLE_UART_CLIENT_LOG);
    }
}

/**
 * @brief		SELSCAN收到数据回调
 * @param[in]	seek_result_data：收到数据相关的结构体变量
 * @return      none
 */
static void sle_uart_client_sample_seek_result_info_cbk(sle_seek_result_info_t *seek_result_data)
{
    /* 指针判空保护（demo放在下面，应该放在最上面，不然打印一用就完蛋） */
    if (seek_result_data == NULL)
    {
        osal_printk("status error\r\n");
        return;
    }

    /* 打印SCAN到的ADV数据 */
    osal_printk("%s sle uart scan data :", SLE_UART_CLIENT_LOG);
    for (uint8_t i = 0; i < seek_result_data->data_length; i++)
    {
        osal_printk("0x%02x ", seek_result_data->data[i]);
    }
    osal_printk("\r\n");

    /* 收到的数据和目标设备的作对比 */
    if (strstr((const char *)seek_result_data->data, SLE_UART_SERVER_NAME) != NULL)
    {
        /* 如果对上了，把MAC保存到g_sle_uart_remote_addr中，待会儿连接使用 */
        memcpy_s(&g_sle_uart_remote_addr, sizeof(sle_addr_t), &seek_result_data->addr, sizeof(sle_addr_t));
        /* 停止SCAN */
        sle_stop_seek();
    }
}

/**
 * @brief		SEL SCAN停止回调
 * @param[in]	status：状态
 * @return      none
 */
static void sle_uart_client_sample_seek_disable_cbk(errcode_t status)
{
    if (status != 0)
    {
        osal_printk("%s sle_uart_client_sample_seek_disable_cbk,status error = %x\r\n", SLE_UART_CLIENT_LOG, status);
    }
    else
    {
        /* 删除之前配对过的设备（为了避免冲突？） */
        sle_remove_paired_remote_device(&g_sle_uart_remote_addr);
        /* 连接设备 */
        sle_connect_remote_device(&g_sle_uart_remote_addr);
    }
}

/**
 * @brief		SEL设备回调函数注册
 * @param[in]	conn_id：连接ID
 * @param[in]	addr：mac 地址
 * @param[in]	errcode_t：错误码
 * @return      none
 */
void sle_uart_client_sample_dev_cbk_register(void)
{
    g_sle_dev_mgr_cbk.sle_power_on_cb = sle_uart_client_sample_sle_power_on_cbk;
    g_sle_dev_mgr_cbk.sle_enable_cb = sle_uart_client_sample_sle_enable_cbk;
    sle_dev_manager_register_callbacks(&g_sle_dev_mgr_cbk);
#if (CORE_NUMS < 2)
    enable_sle();
#endif
}

/**
 * @brief		SLE client SCAN相关cb注册
 * @param[in]   none
 * @return      none
 */
static void sle_uart_client_sample_seek_cbk_register(void)
{
    g_sle_uart_seek_cbk.seek_enable_cb = sle_uart_client_sample_seek_enable_cbk;    /* 开启SCAN回调 */
    g_sle_uart_seek_cbk.seek_result_cb = sle_uart_client_sample_seek_result_info_cbk;   /* SCAN收到一包数据回调 */
    g_sle_uart_seek_cbk.seek_disable_cb = sle_uart_client_sample_seek_disable_cbk;  /* SCAN停止回调 */
    sle_announce_seek_register_callbacks(&g_sle_uart_seek_cbk);
}

#ifdef CONFIG_SAMPLE_SUPPORT_LOW_LATENCY_TYPE
static void sle_uart_client_sample_set_phy_param(void)
{
#ifdef CONFIG_SAMPLE_SUPPORT_PERFORMANCE_TYPE
    sle_set_phy_t param = {0};
    param.tx_format = 1;         // 0 :无线帧类型1(GFSK); 1:无线帧类型2(QPSK)
    param.rx_format = 1;         //
    param.tx_phy = 2;            // 0：1M; 1:2M; 2:4M;
    param.rx_phy = 2;            //
    param.tx_pilot_density = 0x2;  // 导频密度16:1
    param.rx_pilot_density = 0x2;  // 导频密度16:1
    param.g_feedback = 0;
    param.t_feedback = 0;
    if (sle_set_phy_param(get_g_sle_uart_conn_id(), &param) != 0)
    {
        osal_printk("%s sle_set_phy_param fail\r\n", SLE_UART_CLIENT_LOG);
        return;
    }
    osal_printk("%s sle_set_phy_param success\r\n", SLE_UART_CLIENT_LOG);
#else
    // 非跑流sample使用原phy参数
#endif
    return;
}
#endif

/**
 * @brief		SLE client 连接状态改变
 * @param[in]   conn_id    连接 ID。
 * @param[in]   addr       地址。
 * @param[in]   conn_state 连接状态 { @ref sle_acb_state_t }。
 * @param[in]   pair_state 配对状态 { @ref sle_pair_state_t }。
 * @param[in]   disc_reason 断链原因 { @ref sle_disc_reason_t }。
 * @return      none
 */
static void sle_uart_client_sample_connect_state_changed_cbk(uint16_t conn_id, const sle_addr_t *addr,
                                                             sle_acb_state_t conn_state, sle_pair_state_t pair_state,
                                                             sle_disc_reason_t disc_reason)
{
    unused(addr);
    unused(pair_state);
    osal_printk("%s conn state changed disc_reason:0x%x\r\n", SLE_UART_CLIENT_LOG, disc_reason);
    g_sle_uart_conn_id = conn_id;
    /* 已经连上 */
    if (conn_state == SLE_ACB_STATE_CONNECTED)
    {
        osal_printk("%s SLE_ACB_STATE_CONNECTED\r\n", SLE_UART_CLIENT_LOG);
        /* 配对状态是未配对 */
        if (pair_state == SLE_PAIR_NONE)
        {
            /* 开始配对 */
            sle_pair_remote_device(&g_sle_uart_remote_addr);
        }
#ifdef CONFIG_SAMPLE_SUPPORT_LOW_LATENCY_TYPE
        sle_uart_client_sample_set_phy_param();
        osal_msleep(SLE_UART_TASK_DELAY_MS);
        sle_low_latency_rx_enable();
        sle_low_latency_set(get_g_sle_uart_conn_id(), true, SLE_UART_LOW_LATENCY_2K);
        osal_printk("%s sle_low_latency_rx_enable \r\n", SLE_UART_CLIENT_LOG);      //这句话应该在这儿，不应该放外面
#endif  
    }
    /* 未连接（还能有未连接？怎么会有这种状态？） */
    else if (conn_state == SLE_ACB_STATE_NONE)
    {
        osal_printk("%s SLE_ACB_STATE_NONE\r\n", SLE_UART_CLIENT_LOG);
    }
    /* 断连 */
    else if (conn_state == SLE_ACB_STATE_DISCONNECTED)
    {
        osal_printk("%s SLE_ACB_STATE_DISCONNECTED\r\n", SLE_UART_CLIENT_LOG);
        /* 移除配对设备 */
        sle_remove_paired_remote_device(&g_sle_uart_remote_addr);
        /* 再次开启SCAN */
        sle_uart_start_scan();
    }
    /* 其他未知情况 */
    else
    {
        osal_printk("%s status error\r\n", SLE_UART_CLIENT_LOG);
    }
}

/**
 * @brief		SLE client pair完成回调
 * @param[in]   conn_id 连接 ID。
 * @param[in]   addr    地址。
 * @param[in]   status  执行结果错误码。
 * @return      none
 */
void  sle_uart_client_sample_pair_complete_cbk(uint16_t conn_id, const sle_addr_t *addr, errcode_t status)
{
    osal_printk("%s pair complete conn_id:%d, addr:%02x***%02x%02x\n", SLE_UART_CLIENT_LOG, conn_id,
                addr->addr[0], addr->addr[4], addr->addr[5]);
    if (status == 0)
    {
        ssap_exchange_info_t info = {0};
        info.mtu_size = SLE_MTU_SIZE_DEFAULT;
        info.version = 1;
        /* 请求交换SSAP信息 */
        ssapc_exchange_info_req(0, g_sle_uart_conn_id, &info);
    }
}

/**
 * @brief		SLE client 连接相关cb注册
 * @param[in]   none
 * @return      none
 */
static void sle_uart_client_sample_connect_cbk_register(void)
{
    g_sle_uart_connect_cbk.connect_state_changed_cb = sle_uart_client_sample_connect_state_changed_cbk; /* 连接状态改变回调 */
    g_sle_uart_connect_cbk.pair_complete_cb =  sle_uart_client_sample_pair_complete_cbk;                /* pair完成回调 */
    sle_connection_register_callbacks(&g_sle_uart_connect_cbk);
}

static void sle_uart_client_sample_exchange_info_cbk(uint8_t client_id, uint16_t conn_id, ssap_exchange_info_t *param,
                                                     errcode_t status)
{
    osal_printk("%s exchange_info_cbk,pair complete client id:%d status:%d\r\n",
                SLE_UART_CLIENT_LOG, client_id, status);
    osal_printk("%s exchange mtu, mtu size: %d, version: %d.\r\n", SLE_UART_CLIENT_LOG,
                param->mtu_size, param->version);
    ssapc_find_structure_param_t find_param = { 0 };
    find_param.type = SSAP_FIND_TYPE_PROPERTY;
    find_param.start_hdl = 1;
    find_param.end_hdl = 0xFFFF;
    /* 开始查找SSAP服务、特征值等 */
    ssapc_find_structure(0, conn_id, &find_param);
}

static void sle_uart_client_sample_find_structure_cbk(uint8_t client_id, uint16_t conn_id,
                                                      ssapc_find_service_result_t *service,
                                                      errcode_t status)
{
    osal_printk("%s find structure cbk client: %d conn_id:%d status: %d \r\n", SLE_UART_CLIENT_LOG,
                client_id, conn_id, status);
    osal_printk("%s find structure start_hdl:[0x%02x], end_hdl:[0x%02x], uuid len:%d\r\n", SLE_UART_CLIENT_LOG,
                service->start_hdl, service->end_hdl, service->uuid.len);
    g_sle_uart_find_service_result.start_hdl = service->start_hdl;
    g_sle_uart_find_service_result.end_hdl = service->end_hdl;
    memcpy_s(&g_sle_uart_find_service_result.uuid, sizeof(sle_uuid_t), &service->uuid, sizeof(sle_uuid_t));
}

static void sle_uart_client_sample_find_property_cbk(uint8_t client_id, uint16_t conn_id,
                                                     ssapc_find_property_result_t *property, errcode_t status)
{
    osal_printk("%s sle_uart_client_sample_find_property_cbk, client id: %d, conn id: %d, operate ind: %d, "
                "descriptors count: %d status:%d property->handle %d\r\n", SLE_UART_CLIENT_LOG,
                client_id, conn_id, property->operate_indication,
                property->descriptors_count, status, property->handle);
    g_sle_uart_send_param.handle = property->handle;
    g_sle_uart_send_param.type = SSAP_PROPERTY_TYPE_VALUE;
}

static void sle_uart_client_sample_find_structure_cmp_cbk(uint8_t client_id, uint16_t conn_id,
                                                          ssapc_find_structure_result_t *structure_result,
                                                          errcode_t status)
{
    unused(conn_id);
    osal_printk("%s sle_uart_client_sample_find_structure_cmp_cbk,client id:%d status:%d type:%d uuid len:%d \r\n",
                SLE_UART_CLIENT_LOG, client_id, status, structure_result->type, structure_result->uuid.len);
}

/**
 * @brief		SLE client 收到写响应的回调函数（write数据成功回调）
 * @param[in]   client_id    客户端 ID。
 * @param[in]   conn_id      连接 ID。
 * @param[in]   write_result 写结果。
 * @param[in]   status       执行结果错误码。
 * @return      none
 */
static void sle_uart_client_sample_write_cfm_cb(uint8_t client_id, uint16_t conn_id,
                                                ssapc_write_result_t *write_result, errcode_t status)
{
    osal_printk("%s sle_uart_client_sample_write_cfm_cb, conn_id:%d client id:%d status:%d handle:%02x type:%02x\r\n",
                SLE_UART_CLIENT_LOG, conn_id, client_id, status, write_result->handle, write_result->type);
}

/**
 * @brief		SLE client ssap服务注册的回调
 * @param[in]   notification_cb：收到notify数据的回调
 * @param[in]   indication_cb：收到indicate数据的回调
 * @return      none
 */
static void sle_uart_client_sample_ssapc_cbk_register(ssapc_notification_callback notification_cb,
                                                      ssapc_notification_callback indication_cb)
{
    g_sle_uart_ssapc_cbk.exchange_info_cb = sle_uart_client_sample_exchange_info_cbk;   /* mtu改变的回调 */
    g_sle_uart_ssapc_cbk.find_structure_cb = sle_uart_client_sample_find_structure_cbk; /* 发现服务的回调 */
    g_sle_uart_ssapc_cbk.ssapc_find_property_cbk = sle_uart_client_sample_find_property_cbk;    /* 发现特征值的回调 */
    g_sle_uart_ssapc_cbk.find_structure_cmp_cb = sle_uart_client_sample_find_structure_cmp_cbk; /* 发现全部特征值完成的回调 */
    g_sle_uart_ssapc_cbk.write_cfm_cb = sle_uart_client_sample_write_cfm_cb;    /* 收到写响应的回调函数，可以认为发送成功 */
    g_sle_uart_ssapc_cbk.notification_cb = notification_cb;
    g_sle_uart_ssapc_cbk.indication_cb = indication_cb;
    ssapc_register_callbacks(&g_sle_uart_ssapc_cbk);
}

#ifdef CONFIG_SAMPLE_SUPPORT_LOW_LATENCY_TYPE
#include "uart.h"
#ifdef CONFIG_SAMPLE_SUPPORT_PERFORMANCE_TYPE
#include "tcxo.h"
static uint32_t g_sle_recv_count = 0;
static uint64_t g_sle_recv_start_time = 0;
static uint64_t g_sle_recv_end_time = 0;
static uint64_t g_sle_recv_param[2] = { 0 };
#endif
void sle_uart_client_low_latency_recv_data_cbk(uint16_t len, uint8_t *value)
{
#ifdef CONFIG_SAMPLE_SUPPORT_PERFORMANCE_TYPE
    static uint64_t sle_throughput = 0;
    if (value == NULL || len == 0)
    {
        return;
    }
    g_sle_recv_count++;
    if (g_sle_recv_count == 1)
    {
        g_sle_recv_start_time = uapi_tcxo_get_us();
    }
    else if (g_sle_recv_count == SLE_UART_RECV_CNT)
    {
        g_sle_recv_end_time = uapi_tcxo_get_us();
        g_sle_recv_param[0] = g_sle_recv_count;
        g_sle_recv_param[1] = g_sle_recv_end_time - g_sle_recv_start_time;
        g_sle_recv_count = 0;
        g_sle_recv_end_time = 0;
        g_sle_recv_start_time = 0;
        uint64_t tmp;
        tmp = g_sle_recv_param[1] / 1000; // 1000 代表us转化成ms
        sle_throughput = len * SLE_UART_RECV_CNT * 8 / tmp; // 8 代表1byte = 8bit
        osal_printk("recv_len = %d, recv_count = %llu\r\n", len, g_sle_recv_param[0]);
        osal_printk("diff time:%lluus, throughput:%llukbps\r\n", g_sle_recv_param[1], sle_throughput);
    }
#else
    osal_printk("uart recv low latency data:\r\n");
    uapi_uart_write(CONFIG_SLE_UART_BUS, value, len, 0);
#endif
}

void sle_uart_client_low_latency_recv_data_cbk_register(void)
{
    osal_printk("uart recv low latency data register success\r\n");
    sle_low_latency_rx_callbacks_t cbk_func = { NULL };
    cbk_func.low_latency_rx_cb = (low_latency_general_rx_callback)sle_uart_client_low_latency_recv_data_cbk;
    sle_low_latency_rx_register_callbacks(&cbk_func);
}
#endif

/**
 * @brief		SLE client 使用的各种cb注册
 * @param[in]   notification_cb：收到notify数据的回调
 * @param[in]   indication_cb：收到indicate数据的回调
 * @return      none
 */
void sle_uart_client_init(ssapc_notification_callback notification_cb, ssapc_indication_callback indication_cb)
{
    sle_uart_client_sample_seek_cbk_register(); /* SCAN相关的cb注册 */
    sle_uart_client_sample_connect_cbk_register();  /* 连接先关的cb注册 */
    sle_uart_client_sample_ssapc_cbk_register(notification_cb, indication_cb);  /* SSAP服务相关的cb注册 */
#ifdef CONFIG_SAMPLE_SUPPORT_LOW_LATENCY_TYPE
    sle_uart_client_low_latency_recv_data_cbk_register();
#endif
}