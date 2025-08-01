/*
 * Copyright 2024-2025 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_ETHERNET_ETH_NXP_IMX_NETC_PRIV_H_
#define ZEPHYR_DRIVERS_ETHERNET_ETH_NXP_IMX_NETC_PRIV_H_

#include "nxp_imx_netc.h"
#include "fsl_netc_endpoint.h"
#ifndef CONFIG_ETH_NXP_IMX_NETC_MSI_GIC
#include "fsl_msgintr.h"
#endif

/* Buffer and descriptor alignment */
#define NETC_BUFF_ALIGN 64
#define NETC_RX_RING_BUF_SIZE_ALIGN                                                                \
	SDK_SIZEALIGN(CONFIG_ETH_NXP_IMX_RX_RING_BUF_SIZE, NETC_BUFF_ALIGN)

/* MSIX definitions */
#define NETC_TX_MSIX_ENTRY_IDX 0
#define NETC_RX_MSIX_ENTRY_IDX 1
#define NETC_MSIX_ENTRY_NUM    2

#define NETC_MSIX_EVENTS_COUNT      NETC_MSIX_ENTRY_NUM
#define NETC_TX_INTR_MSG_DATA_START 0
#define NETC_RX_INTR_MSG_DATA_START 16
#define NETC_DRV_MAX_INST_SUPPORT   16

/* MSGINTR */
#define NETC_MSGINTR_CHANNEL 0

#if DT_IRQ_HAS_IDX(DT_NODELABEL(netc), 0)
#define NETC_MSGINTR_IRQ DT_IRQN_BY_IDX(DT_NODELABEL(netc), 0)
#endif

#ifndef CONFIG_ETH_NXP_IMX_NETC_MSI_GIC
#if (CONFIG_ETH_NXP_IMX_MSGINTR == 1)
#define NETC_MSGINTR MSGINTR1
#ifndef NETC_MSGINTR_IRQ
#define NETC_MSGINTR_IRQ MSGINTR1_IRQn
#endif
#elif (CONFIG_ETH_NXP_IMX_MSGINTR == 2)
#define NETC_MSGINTR MSGINTR2
#ifndef NETC_MSGINTR_IRQ
#define NETC_MSGINTR_IRQ MSGINTR2_IRQn
#endif
#else
#error "Current CONFIG_ETH_NXP_IMX_MSGINTR not support"
#endif
#endif /* CONFIG_ETH_NXP_IMX_NETC_MSI_GIC */

/* Timeout for various operations */
#define NETC_TIMEOUT K_MSEC(20)

/* Helper function to generate an Ethernet MAC address for a given ENETC instance */
#define FREESCALE_OUI_B0 0x00
#define FREESCALE_OUI_B1 0x04
#define FREESCALE_OUI_B2 0x9f

#define _NETC_GENERATE_MAC_ADDRESS_RANDOM                                                          \
	gen_random_mac(mac_addr, FREESCALE_OUI_B0, FREESCALE_OUI_B1, FREESCALE_OUI_B2)

#define _NETC_GENERATE_MAC_ADDRESS_UNIQUE(n)                                                       \
	do {                                                                                       \
		uint32_t id = 0x001100;                                                            \
                                                                                                   \
		mac_addr[0] = FREESCALE_OUI_B0;                                                    \
		mac_addr[1] = FREESCALE_OUI_B1;                                                    \
		/* Set MAC address locally administered bit (LAA) */                               \
		mac_addr[2] = FREESCALE_OUI_B2 | 0x02;                                             \
		mac_addr[3] = (id >> 16) & 0xff;                                                   \
		mac_addr[4] = (id >> 8) & 0xff;                                                    \
		mac_addr[5] = (id + n) & 0xff;                                                     \
	} while (0)

#define NETC_GENERATE_MAC_ADDRESS(n)                                                               \
	static void netc_eth##n##_generate_mac(uint8_t mac_addr[6])                                \
	{                                                                                          \
		COND_CODE_1(DT_INST_PROP(n, zephyr_random_mac_address),                            \
			    (_NETC_GENERATE_MAC_ADDRESS_RANDOM),                                   \
			    (COND_CODE_0(DT_INST_NODE_HAS_PROP(n, local_mac_address),              \
					 (_NETC_GENERATE_MAC_ADDRESS_UNIQUE(n)),                   \
					 (ARG_UNUSED(mac_addr)))));                      \
	}

struct netc_eth_config {
	DEVICE_MMIO_NAMED_ROM(port);
	DEVICE_MMIO_NAMED_ROM(pfconfig);
	uint16_t si_idx;
	const struct device *phy_dev;
	netc_hw_mii_mode_t phy_mode;
	volatile bool pseudo_mac;
	void (*generate_mac)(uint8_t *mac_addr);
	void (*bdr_init)(netc_bdr_config_t *bdr_config, netc_rx_bdr_config_t *rx_bdr_config,
			 netc_tx_bdr_config_t *tx_bdr_config);
	const struct pinctrl_dev_config *pincfg;
#ifdef CONFIG_ETH_NXP_IMX_NETC_MSI_GIC
	const struct device *msi_dev;
	uint8_t msi_device_id; /* MSI device ID */
#else
	uint8_t tx_intr_msg_data;
	uint8_t rx_intr_msg_data;
#endif
#ifdef CONFIG_PTP_CLOCK_NXP_NETC
	const struct device *ptp_clock;
#endif
};

typedef uint8_t rx_buffer_t[NETC_RX_RING_BUF_SIZE_ALIGN];

struct netc_eth_data {
	DEVICE_MMIO_NAMED_RAM(port);
	DEVICE_MMIO_NAMED_RAM(pfconfig);
	ep_handle_t handle;
	struct net_if *iface;
	uint8_t mac_addr[6];
	/* TX */
	struct k_mutex tx_mutex;
	uint8_t *tx_buff;
	volatile bool tx_done;
	/* RX */
	struct k_sem rx_sem;
	struct k_thread rx_thread;

	K_KERNEL_STACK_MEMBER(rx_thread_stack, CONFIG_ETH_NXP_IMX_RX_THREAD_STACK_SIZE);
	uint8_t *rx_frame;
#ifdef CONFIG_ETH_NXP_IMX_NETC_MSI_GIC
	unsigned int tx_intid;
	unsigned int rx_intid;
#endif
};

int netc_eth_init_common(const struct device *dev);
int netc_eth_tx(const struct device *dev, struct net_pkt *pkt);
enum ethernet_hw_caps netc_eth_get_capabilities(const struct device *dev);
int netc_eth_set_config(const struct device *dev, enum ethernet_config_type type,
			const struct ethernet_config *config);
#ifdef CONFIG_PTP_CLOCK_NXP_NETC
const struct device *netc_eth_get_ptp_clock(const struct device *dev);
#endif
#endif /* ZEPHYR_DRIVERS_ETHERNET_ETH_NXP_IMX_NETC_PRIV_H_ */
