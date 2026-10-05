/**
 * @file    storage_layout.h
 * @brief   AT24C02 V1地址空间的唯一来源。
 * @details 修改任一地址或记录长度必须同时提高配置格式版本并执行迁移评审。
 * @version 0.1.0
 * @date    2026-10-05
 *
 * 关联需求：REQ-DATA-001。
 * 关联设计：DES-STORAGE-001。
 */
#ifndef STORAGE_LAYOUT_H
#define STORAGE_LAYOUT_H

/** 配置双副本A，长度32字节。 */
#define STORAGE_CONFIG_COPY_A_ADDRESS 0x00U
/** 配置双副本B，长度32字节。 */
#define STORAGE_CONFIG_COPY_B_ADDRESS 0x20U
#define STORAGE_CONFIG_RECORD_SIZE    32U /**< 每份配置固定长度，单位字节。 */
#define STORAGE_CARD_BASE_ADDRESS     0x40U /**< 8条4字节UID记录的起始地址。 */
#define STORAGE_CARD_RECORD_SIZE      6U /**< used + UID[4] + CRC8。 */
#define STORAGE_FINGER_BASE_ADDRESS   0x70U /**< 指纹模板映射记录起始地址。 */
#define STORAGE_FINGER_RECORD_SIZE    4U /**< used + template_id_le16 + CRC8。 */
#define STORAGE_TOTAL_SIZE            256U /**< AT24C02可用物理容量。 */

#endif
