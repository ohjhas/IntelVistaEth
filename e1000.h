/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 1999 - 2018 Intel Corporation. */

#ifndef _E1000_H_
#define _E1000_H_

#include "osdep.h"
#include "hw.h"

#define E1000EV_DRIVER_VERSION_MAJOR	1
#define E1000EV_DRIVER_VERSION_MINOR	1
#define E1000EV_POOL_TAG		'v1Ee'

#define E1000_MIN_RING		80
#define E1000_MAX_RING		2048
#define E1000_DEFAULT_TX_RING	512
#define E1000_DEFAULT_RX_RING	256

#define E1000_RX_BUFFER_SIZE	2048
#define E1000_TX_BUFFER_SIZE	4096
#define E1000_MAX_MCAST_LIST	32
#define E1000_WATCHDOG_MS	2000

#define E1000_LSO_MAX_SIZE	64000
#define E1000_LSO_MIN_SEGMENTS	2

#define E1000_DEFAULT_TIDV	8
#define E1000_DEFAULT_TADV	32
#define E1000_DEFAULT_RDTR	0
#define E1000_DEFAULT_RADV	8
#define E1000_ITR_ADAPTIVE	65535
#define E1000_ITR_START		20000

#define E1000_EEPROM_APME	0x0400
#define E1000_FC_PAUSE_TIME	0x0680
#define E1000_MNG_VLAN_NONE	0xFFFF

#define PCICFG_DESC_RING_STATUS	0xe4
#define FLUSH_DESC_REQUIRED	0x100

#define E1000_TIDV_FPD		BIT(31)
#define E1000_RDTR_FPD		BIT(31)

#ifndef E1000_RXD_STAT_IPCS
#define E1000_RXD_STAT_IPCS	0x40
#endif
#ifndef E1000_RXCSUM_IPOFL
#define E1000_RXCSUM_IPOFL	0x00000100
#endif

#define FLAG_HAS_AMT                      BIT(0)
#define FLAG_HAS_FLASH                    BIT(1)
#define FLAG_HAS_HW_VLAN_FILTER           BIT(2)
#define FLAG_HAS_WOL                      BIT(3)
#define FLAG_HAS_CTRLEXT_ON_LOAD          BIT(5)
#define FLAG_HAS_SWSM_ON_LOAD             BIT(6)
#define FLAG_HAS_JUMBO_FRAMES             BIT(7)
#define FLAG_READ_ONLY_NVM                BIT(8)
#define FLAG_IS_ICH                       BIT(9)
#define FLAG_HAS_MSIX                     BIT(10)
#define FLAG_HAS_SMART_POWER_DOWN         BIT(11)
#define FLAG_IS_QUAD_PORT_A               BIT(12)
#define FLAG_IS_QUAD_PORT                 BIT(13)
#define FLAG_HAS_HW_TIMESTAMP             BIT(14)
#define FLAG_APME_IN_WUC                  BIT(15)
#define FLAG_APME_IN_CTRL3                BIT(16)
#define FLAG_APME_CHECK_PORT_B            BIT(17)
#define FLAG_DISABLE_FC_PAUSE_TIME        BIT(18)
#define FLAG_NO_WAKE_UCAST                BIT(19)
#define FLAG_MNG_PT_ENABLED               BIT(20)
#define FLAG_RESET_OVERWRITES_LAA         BIT(21)
#define FLAG_TARC_SPEED_MODE_BIT          BIT(22)
#define FLAG_TARC_SET_BIT_ZERO            BIT(23)
#define FLAG_RX_NEEDS_RESTART             BIT(24)
#define FLAG_LSC_GIG_SPEED_DROP           BIT(25)
#define FLAG_SMART_POWER_DOWN             BIT(26)
#define FLAG_MSI_ENABLED                  BIT(27)
#define FLAG_TSO_FORCE                    BIT(29)
#define FLAG_RESTART_NOW                  BIT(30)
#define FLAG_MSI_TEST_FAILED              BIT(31)

#define FLAG2_CRC_STRIPPING               BIT(0)
#define FLAG2_HAS_PHY_WAKEUP              BIT(1)
#define FLAG2_IS_DISCARDING               BIT(2)
#define FLAG2_DISABLE_ASPM_L1             BIT(3)
#define FLAG2_HAS_PHY_STATS               BIT(4)
#define FLAG2_HAS_EEE                     BIT(5)
#define FLAG2_DMA_BURST                   BIT(6)
#define FLAG2_DISABLE_ASPM_L0S            BIT(7)
#define FLAG2_DISABLE_AIM                 BIT(8)
#define FLAG2_CHECK_PHY_HANG              BIT(9)
#define FLAG2_NO_DISABLE_RX               BIT(10)
#define FLAG2_PCIM2PCI_ARBITER_WA         BIT(11)
#define FLAG2_DFLT_CRC_STRIPPING          BIT(12)
#define FLAG2_CHECK_RX_HWTSTAMP           BIT(13)
#define FLAG2_CHECK_SYSTIM_OVERFLOW       BIT(14)
#define FLAG2_ENABLE_S0IX_FLOWS           BIT(15)
#define FLAG2_DISABLE_K1                  BIT(16)

enum e1000_state_t {
	__E1000_TESTING,
	__E1000_RESETTING,
	__E1000_ACCESS_SHARED_RESOURCE,
	__E1000_DOWN
};

enum e1000_ndis_state {
	E1000_NDIS_PAUSED = 0,
	E1000_NDIS_PAUSING,
	E1000_NDIS_RUNNING
};

enum latency_range {
	lowest_latency = 0,
	low_latency = 1,
	bulk_latency = 2,
	latency_invalid = 255
};

struct e1000_config {
	u32	speed_duplex;
	u32	flow_control;
	u32	tx_ring;
	u32	rx_ring;
	u32	jumbo;
	u32	int_moderation;
	u32	itr;
	u32	priority_vlan;
	u32	eee;
	u32	wake_magic;
	u32	wake_s5;
	u32	ip4_csum;
	u32	tcp4_csum;
	u32	udp4_csum;
	u32	tcp6_csum;
	u32	udp6_csum;
	u32	lsov1_ip4;
	u32	lsov2_ip4;
	u32	lsov2_ip6;
};

#define E1000_CSUM_TX	1
#define E1000_CSUM_RX	2

struct e1000_offload_state {
	u8	tx_ip4, rx_ip4;
	u8	tx_tcp4, rx_tcp4;
	u8	tx_udp4, rx_udp4;
	u8	tx_tcp6, rx_tcp6;
	u8	tx_udp6, rx_udp6;
	u8	lsov1;
	u8	lsov2_ip4, lsov2_ip6;
};

struct e1000_ring {
	void			*desc;
	NDIS_PHYSICAL_ADDRESS	dma;
	u32			size;
	u32			count;
	u32			next_to_use;
	u32			next_to_clean;
	u8 __iomem		*head;
	u8 __iomem		*tail;
};

struct e1000_tx_buffer {
	void			*va;
	NDIS_PHYSICAL_ADDRESS	pa;
	u32			next_to_watch;
	u32			bytecount;
	u32			segs;
};

struct e1000_rx_buffer {
	SINGLE_LIST_ENTRY	link;
	struct e1000_adapter	*adapter;
	void			*va;
	NDIS_PHYSICAL_ADDRESS	pa;
	PMDL			mdl;
	PNET_BUFFER_LIST	nbl;
	struct e1000_rx_buffer	*chain;
};

#define E1000_NBL_RXBUF(nbl)	((struct e1000_rx_buffer *)(nbl)->MiniportReserved[0])

struct e1000_info;

struct e1000_adapter {
	struct e1000_hw		hw;
	struct pci_dev		*pdev;
	const struct e1000_info	*ei;
	LONG volatile		state;
	unsigned int		flags;
	unsigned int		flags2;
	u32			pba;
	u32			max_hw_frame_size;
	u32			max_frame_size;
	u32			min_frame_size;
	u32			mtu;
	u16			eee_advert;
	u16			mng_vlan_id;
	int			int_mode;

	u32			tx_int_delay;
	u32			tx_abs_int_delay;
	u32			rx_int_delay;
	u32			rx_abs_int_delay;
	u32			itr;
	u32			itr_setting;
	u16			tx_itr;
	u16			rx_itr;
	u32			txd_cmd;
	u32			tx_fifo_limit;
	u32			eeprom_wol;
	u32			wol;

	u16			link_speed;
	u16			link_duplex;
	bool			link_up;
	bool			link_indicated;
	bool			hw_ready;

	struct pci_dev		pci;
	struct e1000_config	cfg;

	NDIS_HANDLE		miniport;
	NDIS_HANDLE		interrupt;
	NDIS_HANDLE		dma;
	NDIS_HANDLE		nbl_pool;
	NDIS_HANDLE		watchdog_timer;
	NDIS_HANDLE		watchdog_work;
	NDIS_HANDLE		reset_work;

	PHYSICAL_ADDRESS	mmio_pa;
	ULONG			mmio_len;
	ULONG			flash_len;
	bool			interrupt_registered;
	bool volatile		halting;
	bool volatile		surprise_removed;
	bool volatile		msi_test_ok;

	enum e1000_ndis_state	ndis_state;
	NDIS_DEVICE_POWER_STATE	power_state;
	ULONG			wake_flags;
	LONG volatile		watchdog_queued;
	LONG volatile		reset_pending;
	LONG volatile		ndis_reset_requested;
	u32			watchdog_ticks;

	KMUTEX			hw_mutex;

	struct e1000_offload_state off;
	bool			lso_capable;
	ULONG			encap_ip4;
	ULONG			encap_ip6;
	u16			vlan_id;

	NDIS_SPIN_LOCK		tx_lock;
	struct e1000_ring	tx_ring;
	struct e1000_tx_buffer	*tx_buf;
	PNET_BUFFER_LIST	tx_wait_head;
	PNET_BUFFER_LIST	tx_wait_tail;

	NDIS_SPIN_LOCK		rx_lock;
	struct e1000_ring	rx_ring;
	struct e1000_rx_buffer	**rx_slot;
	struct e1000_rx_buffer	*rx_bufs;
	u32			rx_buf_count;
	SINGLE_LIST_ENTRY	rx_free;
	u32			rx_free_count;
	LONG			rx_outstanding;
	bool			rx_indicating;

	LONG volatile		icr_pending;
	LONG volatile		dpc_queued;
	LONG volatile		dpc_refs;
	u32			total_tx_bytes;
	u32			total_tx_packets;
	u32			total_rx_bytes;
	u32			total_rx_packets;

	u32			tx_hang_ntc;
	u32			tx_hang_ticks;

	NDIS_SPIN_LOCK		filter_lock;
	ULONG			packet_filter;
	u8			mc_list[E1000_MAX_MCAST_LIST * ETH_ALEN];
	u32			mc_count;
	u8			perm_addr[ETH_ALEN];
	u8			curr_addr[ETH_ALEN];

	struct e1000_hw_stats	stats;
	u64			tpt_old;
	u64			colc_old;
	u64			tx_ok;
	u64			tx_bytes;
	u64			tx_ucast;
	u64			tx_mcast;
	u64			tx_bcast;
	u64			tx_ucast_bytes;
	u64			tx_mcast_bytes;
	u64			tx_bcast_bytes;
	u64			tx_dropped;
	u64			tx_lso;
	u64			tx_ring_full;
	u64			rx_ok;
	u64			rx_bytes;
	u64			rx_ucast;
	u64			rx_mcast;
	u64			rx_bcast;
	u64			rx_ucast_bytes;
	u64			rx_mcast_bytes;
	u64			rx_bcast_bytes;
	u64			rx_errors;
	u64			rx_nobuf;
	u64			rx_vlan_dropped;
	u32			resets;
	u32			corr_errors;
	u32			uncorr_errors;
};

struct e1000_info {
	enum e1000_mac_type		mac;
	unsigned int			flags;
	unsigned int			flags2;
	u32				pba;
	u32				max_hw_frame_size;
	s32				(*get_variants)(struct e1000_adapter *);
	const struct e1000_mac_operations *mac_ops;
	const struct e1000_phy_operations *phy_ops;
	const struct e1000_nvm_operations *nvm_ops;
};

extern struct e1000_info e1000_pch_lpt_info;
extern struct e1000_info e1000_pch_spt_info;
extern struct e1000_info e1000_pch_cnp_info;
extern struct e1000_info e1000_pch_tgp_info;
extern struct e1000_info e1000_pch_adp_info;
extern struct e1000_info e1000_pch_mtp_info;
extern struct e1000_info e1000_pch_ptp_info;

void e1000_ich8lan_init_tables(void);

static __inline s32 e1000_phy_hw_reset(struct e1000_hw *hw)
{
	return hw->phy.ops.reset(hw);
}

static __inline s32 e1e_rphy(struct e1000_hw *hw, u32 offset, u16 *data)
{
	return hw->phy.ops.read_reg(hw, offset, data);
}

static __inline s32 e1e_rphy_locked(struct e1000_hw *hw, u32 offset, u16 *data)
{
	return hw->phy.ops.read_reg_locked(hw, offset, data);
}

static __inline s32 e1e_wphy(struct e1000_hw *hw, u32 offset, u16 data)
{
	return hw->phy.ops.write_reg(hw, offset, data);
}

static __inline s32 e1e_wphy_locked(struct e1000_hw *hw, u32 offset, u16 data)
{
	return hw->phy.ops.write_reg_locked(hw, offset, data);
}

void e1000e_reload_nvm_generic(struct e1000_hw *hw);

static __inline s32 e1000e_read_mac_addr(struct e1000_hw *hw)
{
	if (hw->mac.ops.read_mac_addr)
		return hw->mac.ops.read_mac_addr(hw);

	return e1000_read_mac_addr_generic(hw);
}

static __inline s32 e1000_validate_nvm_checksum(struct e1000_hw *hw)
{
	return hw->nvm.ops.validate(hw);
}

static __inline s32 e1000e_update_nvm_checksum(struct e1000_hw *hw)
{
	return hw->nvm.ops.update(hw);
}

static __inline s32 e1000_read_nvm(struct e1000_hw *hw, u16 offset, u16 words,
				   u16 *data)
{
	return hw->nvm.ops.read(hw, offset, words, data);
}

static __inline s32 e1000_write_nvm(struct e1000_hw *hw, u16 offset, u16 words,
				    u16 *data)
{
	return hw->nvm.ops.write(hw, offset, words, data);
}

static __inline s32 e1000_get_phy_info(struct e1000_hw *hw)
{
	return hw->phy.ops.get_info(hw);
}

static __inline u32 __er32(struct e1000_hw *hw, unsigned long reg)
{
	return readl((u8 *)hw->hw_addr + reg);
}

#define er32(reg)	__er32(hw, E1000_##reg)

void __ew32(struct e1000_hw *hw, unsigned long reg, u32 val);

#define ew32(reg, val)	__ew32(hw, E1000_##reg, (val))

#define e1e_flush()	er32(STATUS)

#define E1000_WRITE_REG_ARRAY(a, reg, offset, value) \
	(__ew32((a), (reg + ((offset) << 2)), (value)))

#define E1000_READ_REG_ARRAY(a, reg, offset) \
	(readl((u8 *)(a)->hw_addr + reg + ((offset) << 2)))

#define E1000_TX_DESC(R, i)	(&(((struct e1000_tx_desc *)((R).desc))[i]))
#define E1000_CONTEXT_DESC(R, i) (&(((struct e1000_context_desc *)((R).desc))[i]))
#define E1000_RX_DESC_EXT(R, i)	(&(((union e1000_rx_desc_extended *)((R).desc))[i]))

#define AUTO_ALL_MODES		0

static __inline void e1000_lock(PNDIS_SPIN_LOCK lock, bool dispatch)
{
	if (dispatch) {
		NdisDprAcquireSpinLock(lock);
	} else {
		NdisAcquireSpinLock(lock);
	}
}

static __inline void e1000_unlock(PNDIS_SPIN_LOCK lock, bool dispatch)
{
	if (dispatch) {
		NdisDprReleaseSpinLock(lock);
	} else {
		NdisReleaseSpinLock(lock);
	}
}

void e1000e_reset(struct e1000_adapter *adapter);
void e1000e_up(struct e1000_adapter *adapter);
void e1000e_down(struct e1000_adapter *adapter, bool reset);
void e1000_configure(struct e1000_adapter *adapter);
void e1000_irq_disable(struct e1000_adapter *adapter);
void e1000_irq_enable(struct e1000_adapter *adapter);
void e1000e_get_hw_control(struct e1000_adapter *adapter);
void e1000e_release_hw_control(struct e1000_adapter *adapter);
void e1000e_power_up_phy(struct e1000_adapter *adapter);
void e1000_power_down_phy(struct e1000_adapter *adapter);
void e1000e_write_itr(struct e1000_adapter *adapter, u32 itr);
void e1000_set_itr(struct e1000_adapter *adapter);
void e1000e_set_rx_mode(struct e1000_adapter *adapter);
void e1000e_update_stats(struct e1000_adapter *adapter);
bool e1000e_has_link(struct e1000_adapter *adapter);
int  e1000_sw_init(struct e1000_adapter *adapter);
int  e1000_hw_probe(struct e1000_adapter *adapter);
void e1000_apply_link_config(struct e1000_adapter *adapter);
void e1000_suspend(struct e1000_adapter *adapter, u32 wufc);
void e1000_resume(struct e1000_adapter *adapter);

NDIS_STATUS e1000_alloc_rings(struct e1000_adapter *adapter);
void e1000_free_rings(struct e1000_adapter *adapter);
void e1000_alloc_rx_buffers(struct e1000_adapter *adapter);
void e1000_clean_tx_ring(struct e1000_adapter *adapter);
void e1000_clean_rx_ring(struct e1000_adapter *adapter);
void e1000_tx_complete_all_waiting(struct e1000_adapter *adapter,
				   NDIS_STATUS status, bool dispatch);
void e1000_handle_interrupt(struct e1000_adapter *adapter);

MINIPORT_SEND_NET_BUFFER_LISTS     E1000SendNetBufferLists;
MINIPORT_RETURN_NET_BUFFER_LISTS   E1000ReturnNetBufferLists;
MINIPORT_CANCEL_SEND               E1000CancelSend;
MINIPORT_ISR                       E1000Isr;
MINIPORT_INTERRUPT_DPC             E1000InterruptDpc;
MINIPORT_ENABLE_INTERRUPT          E1000EnableInterrupt;
MINIPORT_DISABLE_INTERRUPT         E1000DisableInterrupt;
MINIPORT_MESSAGE_INTERRUPT         E1000MsiIsr;
MINIPORT_MESSAGE_INTERRUPT_DPC     E1000MsiDpc;
MINIPORT_ENABLE_MESSAGE_INTERRUPT  E1000MsiEnable;
MINIPORT_DISABLE_MESSAGE_INTERRUPT E1000MsiDisable;

void e1000_schedule_watchdog(struct e1000_adapter *adapter);
void e1000_schedule_reset(struct e1000_adapter *adapter);
NDIS_STATUS e1000_set_power(struct e1000_adapter *adapter,
			    NDIS_DEVICE_POWER_STATE state);

MINIPORT_OID_REQUEST               E1000OidRequest;
MINIPORT_CANCEL_OID_REQUEST        E1000CancelOidRequest;
extern NDIS_OID e1000_supported_oids[];
extern ULONG e1000_supported_oids_size;
void e1000_indicate_link(struct e1000_adapter *adapter);
void e1000_fill_offload(struct e1000_adapter *adapter, NDIS_OFFLOAD *o,
			bool hardware);
void e1000_fill_pm_caps(struct e1000_adapter *adapter,
			NDIS_PNP_CAPABILITIES *caps);

#endif
