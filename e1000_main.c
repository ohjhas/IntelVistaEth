// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 1999 - 2018 Intel Corporation. */

#include "e1000.h"

DRIVER_INITIALIZE DriverEntry;
MINIPORT_INITIALIZE E1000Initialize;
MINIPORT_HALT E1000Halt;
MINIPORT_UNLOAD E1000Unload;
MINIPORT_PAUSE E1000Pause;
MINIPORT_RESTART E1000Restart;
MINIPORT_CHECK_FOR_HANG E1000CheckForHang;
MINIPORT_RESET E1000Reset;
MINIPORT_DEVICE_PNP_EVENT_NOTIFY E1000DevicePnPEvent;
MINIPORT_SHUTDOWN E1000Shutdown;
MINIPORT_PROCESS_SG_LIST E1000ProcessSGList;
NDIS_TIMER_FUNCTION E1000WatchdogTimer;
NDIS_IO_WORKITEM_FUNCTION E1000WatchdogWork;
NDIS_IO_WORKITEM_FUNCTION E1000ResetWork;

static NDIS_HANDLE e1000_driver_handle;

static const struct {
	u16			device;
	struct e1000_info	*info;
} e1000_pci_tbl[] = {
	{ E1000_DEV_ID_PCH_LPT_I217_LM,		&e1000_pch_lpt_info },
	{ E1000_DEV_ID_PCH_LPT_I217_V,		&e1000_pch_lpt_info },
	{ E1000_DEV_ID_PCH_LPTLP_I218_LM,	&e1000_pch_lpt_info },
	{ E1000_DEV_ID_PCH_LPTLP_I218_V,	&e1000_pch_lpt_info },
	{ E1000_DEV_ID_PCH_I218_LM2,		&e1000_pch_lpt_info },
	{ E1000_DEV_ID_PCH_I218_V2,		&e1000_pch_lpt_info },
	{ E1000_DEV_ID_PCH_I218_LM3,		&e1000_pch_lpt_info },
	{ E1000_DEV_ID_PCH_I218_V3,		&e1000_pch_lpt_info },
	{ E1000_DEV_ID_PCH_SPT_I219_LM,		&e1000_pch_spt_info },
	{ E1000_DEV_ID_PCH_SPT_I219_V,		&e1000_pch_spt_info },
	{ E1000_DEV_ID_PCH_SPT_I219_LM2,	&e1000_pch_spt_info },
	{ E1000_DEV_ID_PCH_SPT_I219_V2,		&e1000_pch_spt_info },
	{ E1000_DEV_ID_PCH_LBG_I219_LM3,	&e1000_pch_spt_info },
	{ E1000_DEV_ID_PCH_SPT_I219_LM4,	&e1000_pch_spt_info },
	{ E1000_DEV_ID_PCH_SPT_I219_V4,		&e1000_pch_spt_info },
	{ E1000_DEV_ID_PCH_SPT_I219_LM5,	&e1000_pch_spt_info },
	{ E1000_DEV_ID_PCH_SPT_I219_V5,		&e1000_pch_spt_info },
	{ E1000_DEV_ID_PCH_CNP_I219_LM6,	&e1000_pch_cnp_info },
	{ E1000_DEV_ID_PCH_CNP_I219_V6,		&e1000_pch_cnp_info },
	{ E1000_DEV_ID_PCH_CNP_I219_LM7,	&e1000_pch_cnp_info },
	{ E1000_DEV_ID_PCH_CNP_I219_V7,		&e1000_pch_cnp_info },
	{ E1000_DEV_ID_PCH_ICP_I219_LM8,	&e1000_pch_cnp_info },
	{ E1000_DEV_ID_PCH_ICP_I219_V8,		&e1000_pch_cnp_info },
	{ E1000_DEV_ID_PCH_ICP_I219_LM9,	&e1000_pch_cnp_info },
	{ E1000_DEV_ID_PCH_ICP_I219_V9,		&e1000_pch_cnp_info },
	{ E1000_DEV_ID_PCH_CMP_I219_LM10,	&e1000_pch_cnp_info },
	{ E1000_DEV_ID_PCH_CMP_I219_V10,	&e1000_pch_cnp_info },
	{ E1000_DEV_ID_PCH_CMP_I219_LM11,	&e1000_pch_cnp_info },
	{ E1000_DEV_ID_PCH_CMP_I219_V11,	&e1000_pch_cnp_info },
	{ E1000_DEV_ID_PCH_CMP_I219_LM12,	&e1000_pch_spt_info },
	{ E1000_DEV_ID_PCH_CMP_I219_V12,	&e1000_pch_spt_info },
	{ E1000_DEV_ID_PCH_TGP_I219_LM13,	&e1000_pch_tgp_info },
	{ E1000_DEV_ID_PCH_TGP_I219_V13,	&e1000_pch_tgp_info },
	{ E1000_DEV_ID_PCH_TGP_I219_LM14,	&e1000_pch_tgp_info },
	{ E1000_DEV_ID_PCH_TGP_I219_V14,	&e1000_pch_tgp_info },
	{ E1000_DEV_ID_PCH_TGP_I219_LM15,	&e1000_pch_tgp_info },
	{ E1000_DEV_ID_PCH_TGP_I219_V15,	&e1000_pch_tgp_info },
	{ E1000_DEV_ID_PCH_RPL_I219_LM23,	&e1000_pch_adp_info },
	{ E1000_DEV_ID_PCH_RPL_I219_V23,	&e1000_pch_adp_info },
	{ E1000_DEV_ID_PCH_ADP_I219_LM16,	&e1000_pch_adp_info },
	{ E1000_DEV_ID_PCH_ADP_I219_V16,	&e1000_pch_adp_info },
	{ E1000_DEV_ID_PCH_ADP_I219_LM17,	&e1000_pch_adp_info },
	{ E1000_DEV_ID_PCH_ADP_I219_V17,	&e1000_pch_adp_info },
	{ E1000_DEV_ID_PCH_RPL_I219_LM22,	&e1000_pch_adp_info },
	{ E1000_DEV_ID_PCH_RPL_I219_V22,	&e1000_pch_adp_info },
	{ E1000_DEV_ID_PCH_ADP_I219_LM19,	&e1000_pch_adp_info },
	{ E1000_DEV_ID_PCH_ADP_I219_V19,	&e1000_pch_adp_info },
	{ E1000_DEV_ID_PCH_MTP_I219_LM18,	&e1000_pch_mtp_info },
	{ E1000_DEV_ID_PCH_MTP_I219_V18,	&e1000_pch_mtp_info },
	{ E1000_DEV_ID_PCH_LNP_I219_LM20,	&e1000_pch_mtp_info },
	{ E1000_DEV_ID_PCH_LNP_I219_V20,	&e1000_pch_mtp_info },
	{ E1000_DEV_ID_PCH_LNP_I219_LM21,	&e1000_pch_mtp_info },
	{ E1000_DEV_ID_PCH_LNP_I219_V21,	&e1000_pch_mtp_info },
	{ E1000_DEV_ID_PCH_ARL_I219_LM24,	&e1000_pch_mtp_info },
	{ E1000_DEV_ID_PCH_ARL_I219_V24,	&e1000_pch_mtp_info },
	{ E1000_DEV_ID_PCH_PTP_I219_LM25,	&e1000_pch_ptp_info },
	{ E1000_DEV_ID_PCH_PTP_I219_V25,	&e1000_pch_ptp_info },
	{ E1000_DEV_ID_PCH_PTP_I219_LM27,	&e1000_pch_ptp_info },
	{ E1000_DEV_ID_PCH_PTP_I219_V27,	&e1000_pch_ptp_info },
	{ E1000_DEV_ID_PCH_NVL_I219_LM29,	&e1000_pch_ptp_info },
	{ E1000_DEV_ID_PCH_NVL_I219_V29,	&e1000_pch_ptp_info },
};

#define E1000_PCI_VENDOR_INTEL		0x8086
#define E1000_PCI_CAP_ID_EXP		0x10
#define E1000_PCI_CMD_MEMORY		0x0002
#define E1000_PCI_CMD_MASTER		0x0004
#define E1000_PCI_STATUS_CAP_LIST	0x0010

static void e1000_lock_hw(struct e1000_adapter *adapter)
{
	KeWaitForSingleObject(&adapter->hw_mutex, Executive, KernelMode,
			      FALSE, NULL);
}

static void e1000_unlock_hw(struct e1000_adapter *adapter)
{
	KeReleaseMutex(&adapter->hw_mutex, FALSE);
}

static u8 e1000_find_pcie_cap(struct e1000_adapter *adapter,
			      PCI_COMMON_CONFIG *cfg)
{
	u8 pos, id;
	int guard = 48;

	if (!(cfg->Status & E1000_PCI_STATUS_CAP_LIST))
		return 0;

	pos = cfg->u.type0.CapabilitiesPtr & ~3;
	while (pos && guard--) {
		if (NdisMGetBusData(adapter->miniport, PCI_WHICHSPACE_CONFIG,
				    pos, &id, 1) != 1)
			return 0;
		if (id == E1000_PCI_CAP_ID_EXP)
			return pos;
		if (NdisMGetBusData(adapter->miniport, PCI_WHICHSPACE_CONFIG,
				    pos + 1, &pos, 1) != 1)
			return 0;
		pos &= ~3;
	}
	return 0;
}

static ULONGLONG e1000_bar_address(PCI_COMMON_CONFIG *cfg, int bar)
{
	ULONG lo = cfg->u.type0.BaseAddresses[bar];
	ULONGLONG addr;

	if (lo & 1)
		return 0;

	addr = lo & ~0xFUL;
	if (((lo >> 1) & 3) == 2 && bar < 5)
		addr |= (ULONGLONG)cfg->u.type0.BaseAddresses[bar + 1] << 32;
	return addr;
}

static u32 e1000_read_int(NDIS_HANDLE config, PWSTR name, u32 def,
			  u32 lo, u32 hi)
{
	PNDIS_CONFIGURATION_PARAMETER param = NULL;
	NDIS_STRING keyword;
	NDIS_STATUS status;
	u32 value;

	NdisInitUnicodeString(&keyword, name);
	NdisReadConfiguration(&status, &param, config, &keyword,
			      NdisParameterInteger);
	if (status != NDIS_STATUS_SUCCESS || !param ||
	    param->ParameterType != NdisParameterInteger)
		return def;

	value = param->ParameterData.IntegerData;
	return (value < lo || value > hi) ? def : value;
}

static void e1000_read_registry(struct e1000_adapter *adapter)
{
	struct e1000_config *c = &adapter->cfg;
	NDIS_CONFIGURATION_OBJECT co;
	NDIS_HANDLE config = NULL;
	NDIS_STATUS status;
	PVOID addr = NULL;
	UINT len = 0;

	c->speed_duplex = 0;
	c->flow_control = 3;
	c->tx_ring = E1000_DEFAULT_TX_RING;
	c->rx_ring = E1000_DEFAULT_RX_RING;
	c->jumbo = ETH_FRAME_LEN;
	c->int_moderation = 1;
	c->itr = E1000_ITR_ADAPTIVE;
	c->priority_vlan = 3;
	c->eee = 1;
	c->wake_magic = 1;
	c->wake_s5 = 0;
	c->ip4_csum = c->tcp4_csum = c->udp4_csum = 3;
	c->tcp6_csum = c->udp6_csum = 3;
	c->lsov1_ip4 = c->lsov2_ip4 = c->lsov2_ip6 = 1;

	NdisZeroMemory(&co, sizeof(co));
	co.Header.Type = NDIS_OBJECT_TYPE_CONFIGURATION_OBJECT;
	co.Header.Revision = NDIS_CONFIGURATION_OBJECT_REVISION_1;
	co.Header.Size = NDIS_SIZEOF_CONFIGURATION_OBJECT_REVISION_1;
	co.NdisHandle = adapter->miniport;
	co.Flags = 0;

	if (NdisOpenConfigurationEx(&co, &config) != NDIS_STATUS_SUCCESS)
		return;

	c->speed_duplex = e1000_read_int(config, L"*SpeedDuplex", 0, 0, 6);
	if (c->speed_duplex == 5)
		c->speed_duplex = 0;
	c->flow_control = e1000_read_int(config, L"*FlowControl", 3, 0, 3);
	c->tx_ring = e1000_read_int(config, L"*TransmitBuffers",
				    E1000_DEFAULT_TX_RING, E1000_MIN_RING,
				    E1000_MAX_RING) & ~7;
	c->rx_ring = e1000_read_int(config, L"*ReceiveBuffers",
				    E1000_DEFAULT_RX_RING, E1000_MIN_RING,
				    E1000_MAX_RING) & ~7;
	c->jumbo = e1000_read_int(config, L"*JumboPacket", ETH_FRAME_LEN,
				  ETH_FRAME_LEN, 9014);
	c->int_moderation = e1000_read_int(config, L"*InterruptModeration",
					   1, 0, 1);
	c->itr = e1000_read_int(config, L"ITR", E1000_ITR_ADAPTIVE, 0, 65535);
	if (c->itr && c->itr != E1000_ITR_ADAPTIVE)
		c->itr = min_t(u32, max_t(u32, c->itr, 100), 100000);
	c->priority_vlan = e1000_read_int(config, L"*PriorityVLANTag", 3, 0, 3);
	c->eee = e1000_read_int(config, L"EEELinkAdvertisement", 1, 0, 1);
	c->wake_magic = e1000_read_int(config, L"*WakeOnMagicPacket", 1, 0, 1);
	c->wake_s5 = e1000_read_int(config, L"WakeFromPowerOff", 0, 0, 1);
	c->ip4_csum = e1000_read_int(config, L"*IPChecksumOffloadIPv4", 3, 0, 3);
	c->tcp4_csum = e1000_read_int(config, L"*TCPChecksumOffloadIPv4", 3, 0, 3);
	c->udp4_csum = e1000_read_int(config, L"*UDPChecksumOffloadIPv4", 3, 0, 3);
	c->tcp6_csum = e1000_read_int(config, L"*TCPChecksumOffloadIPv6", 3, 0, 3);
	c->udp6_csum = e1000_read_int(config, L"*UDPChecksumOffloadIPv6", 3, 0, 3);
	c->lsov1_ip4 = e1000_read_int(config, L"*LsoV1IPv4", 1, 0, 1);
	c->lsov2_ip4 = e1000_read_int(config, L"*LsoV2IPv4", 1, 0, 1);
	c->lsov2_ip6 = e1000_read_int(config, L"*LsoV2IPv6", 1, 0, 1);

	NdisReadNetworkAddress(&status, &addr, &len, config);
	if (status == NDIS_STATUS_SUCCESS && len == ETH_ALEN &&
	    is_valid_ether_addr((const u8 *)addr))
		NdisMoveMemory(adapter->curr_addr, addr, ETH_ALEN);

	NdisCloseConfiguration(config);
}

static void e1000_init_offloads(struct e1000_adapter *adapter)
{
	struct e1000_config *c = &adapter->cfg;
	struct e1000_offload_state *o = &adapter->off;

	adapter->lso_capable = adapter->hw.mac.type != e1000_pch_spt;

	o->tx_ip4 = (c->ip4_csum & E1000_CSUM_TX) != 0;
	o->rx_ip4 = (c->ip4_csum & E1000_CSUM_RX) != 0;
	o->tx_tcp4 = (c->tcp4_csum & E1000_CSUM_TX) != 0;
	o->rx_tcp4 = (c->tcp4_csum & E1000_CSUM_RX) != 0;
	o->tx_udp4 = (c->udp4_csum & E1000_CSUM_TX) != 0;
	o->rx_udp4 = (c->udp4_csum & E1000_CSUM_RX) != 0;
	o->tx_tcp6 = (c->tcp6_csum & E1000_CSUM_TX) != 0;
	o->rx_tcp6 = (c->tcp6_csum & E1000_CSUM_RX) != 0;
	o->tx_udp6 = (c->udp6_csum & E1000_CSUM_TX) != 0;
	o->rx_udp6 = (c->udp6_csum & E1000_CSUM_RX) != 0;
	o->lsov1 = adapter->lso_capable && c->lsov1_ip4;
	o->lsov2_ip4 = adapter->lso_capable && c->lsov2_ip4;
	o->lsov2_ip6 = adapter->lso_capable && c->lsov2_ip6;
}

static void e1000_init_mtu(struct e1000_adapter *adapter)
{
	u32 max_mtu = adapter->max_hw_frame_size - (VLAN_ETH_HLEN + ETH_FCS_LEN);
	u32 mtu = ETH_DATA_LEN;

	if (adapter->cfg.jumbo > ETH_FRAME_LEN) {
		if (adapter->flags & FLAG_HAS_JUMBO_FRAMES)
			mtu = min_t(u32, adapter->cfg.jumbo - ETH_HLEN, max_mtu);
		else
			e_warn("Jumbo Frames not supported on this board\n");
	}

	adapter->mtu = mtu;
	adapter->max_frame_size = mtu + VLAN_ETH_HLEN + ETH_FCS_LEN;
}

static void e1000_set_link_down(struct e1000_adapter *adapter)
{
	if (!adapter->link_up)
		return;

	adapter->link_up = false;
	adapter->link_speed = 0;
	adapter->link_duplex = 0;
	e1000_indicate_link(adapter);
}

static void e1000e_reinit_locked(struct e1000_adapter *adapter)
{
	set_bit(__E1000_RESETTING, &adapter->state);
	adapter->resets++;
	e1000_set_link_down(adapter);
	e1000e_down(adapter, true);
	e1000e_up(adapter);
	clear_bit(__E1000_RESETTING, &adapter->state);
}

static bool e1000_tx_hung(struct e1000_adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	struct e1000_ring *tx_ring = &adapter->tx_ring;
	bool hung = false;

	NdisAcquireSpinLock(&adapter->tx_lock);
	if (tx_ring->next_to_clean != tx_ring->next_to_use &&
	    tx_ring->next_to_clean == adapter->tx_hang_ntc) {
		if (++adapter->tx_hang_ticks >= 3 &&
		    !(er32(STATUS) & E1000_STATUS_TXOFF))
			hung = true;
	} else {
		adapter->tx_hang_ticks = 0;
		adapter->tx_hang_ntc = tx_ring->next_to_clean;
	}
	NdisReleaseSpinLock(&adapter->tx_lock);

	return hung;
}

static bool e1000_tx_pending(struct e1000_adapter *adapter)
{
	bool pending;

	NdisAcquireSpinLock(&adapter->tx_lock);
	pending = adapter->tx_ring.next_to_clean != adapter->tx_ring.next_to_use;
	NdisReleaseSpinLock(&adapter->tx_lock);
	return pending;
}

static void e1000_watchdog_task(struct e1000_adapter *adapter)
{
	struct e1000_mac_info *mac = &adapter->hw.mac;
	struct e1000_phy_info *phy = &adapter->hw.phy;
	u32 dmoff_exit_timeout = 100, tries = 0;
	struct e1000_hw *hw = &adapter->hw;
	u32 tctl, pcim_state;
	bool link;

	link = e1000e_has_link(adapter);
	if (adapter->link_up && link)
		goto link_up;

	if (link) {
		if (!adapter->link_up) {
			if (er32(FWSM) & E1000_ICH_FWSM_FW_VALID) {
				pcim_state = er32(STATUS);
				while (pcim_state & E1000_STATUS_PCIM_STATE) {
					if (tries++ == dmoff_exit_timeout) {
						e_dbg("Error in exiting dmoff\n");
						break;
					}
					usleep_range(10000, 20000);
					pcim_state = er32(STATUS);

					if (!(pcim_state & E1000_STATUS_PCIM_STATE))
						e1000_phy_hw_reset(&adapter->hw);
				}
			}

			mac->ops.get_link_up_info(&adapter->hw,
						  &adapter->link_speed,
						  &adapter->link_duplex);

			e1000e_check_downshift(hw);
			if (phy->speed_downgraded)
				e_warn("Link Speed was downgraded by SmartSpeed\n");

			tctl = er32(TCTL);
			tctl |= E1000_TCTL_EN;
			ew32(TCTL, tctl);

			if (phy->ops.cfg_on_link_up)
				phy->ops.cfg_on_link_up(hw);

			e_info("NIC Link is Up %d Mbps %s Duplex\n",
			       adapter->link_speed,
			       adapter->link_duplex == FULL_DUPLEX ?
			       "Full" : "Half");

			adapter->tx_hang_ticks = 0;
			adapter->link_up = true;
			e1000_indicate_link(adapter);
		}
	} else {
		if (adapter->link_up) {
			e_info("NIC Link is Down\n");
			e1000_set_link_down(adapter);
		}
	}

link_up:
	e1000e_update_stats(adapter);

	mac->tx_packet_delta = (u32)(adapter->stats.tpt - adapter->tpt_old);
	adapter->tpt_old = adapter->stats.tpt;
	mac->collision_delta = (u32)(adapter->stats.colc - adapter->colc_old);
	adapter->colc_old = adapter->stats.colc;

	if (!adapter->link_up && e1000_tx_pending(adapter))
		adapter->flags |= FLAG_RESTART_NOW;

	if (adapter->link_up && e1000_tx_hung(adapter)) {
		e_err("Detected Hardware Unit Hang - resetting\n");
		adapter->flags |= FLAG_RESTART_NOW;
	}

	if (adapter->flags & FLAG_RESTART_NOW) {
		adapter->flags &= ~FLAG_RESTART_NOW;
		e1000e_reinit_locked(adapter);
		return;
	}

	e1000e_update_adaptive(&adapter->hw);

	if (!adapter->link_indicated)
		e1000_indicate_link(adapter);

	if (++adapter->watchdog_ticks % 5 == 0 && adapter->link_up)
		e_info("stats: %u Mbps %s itr %u | tx %I64u drop %I64u full %I64u lso %I64u | rx %I64u err %I64u nobuf %I64u | xoff rx %I64u tx %I64u | mpc %I64u rnbc %I64u crc %I64u | resets %u\n",
		       adapter->link_speed,
		       adapter->link_duplex == FULL_DUPLEX ? "FD" : "HD",
		       adapter->itr, adapter->tx_ok, adapter->tx_dropped,
		       adapter->tx_ring_full, adapter->tx_lso, adapter->rx_ok,
		       adapter->rx_errors, adapter->rx_nobuf,
		       adapter->stats.xoffrxc, adapter->stats.xofftxc,
		       adapter->stats.mpc, adapter->stats.rnbc,
		       adapter->stats.crcerrs, adapter->resets);

	ew32(ICS, E1000_ICS_RXDMT0);
}

VOID E1000WatchdogTimer(PVOID SystemSpecific1, PVOID FunctionContext,
			PVOID SystemSpecific2, PVOID SystemSpecific3)
{
	e1000_schedule_watchdog((struct e1000_adapter *)FunctionContext);
}

void e1000_schedule_watchdog(struct e1000_adapter *adapter)
{
	if (adapter->halting)
		return;
	if (InterlockedCompareExchange(&adapter->watchdog_queued, 1, 0) == 0)
		NdisQueueIoWorkItem(adapter->watchdog_work, E1000WatchdogWork,
				    adapter);
}

VOID E1000WatchdogWork(PVOID WorkItemContext, NDIS_HANDLE NdisIoWorkItemHandle)
{
	struct e1000_adapter *adapter = WorkItemContext;

	e1000_lock_hw(adapter);
	if (!adapter->halting && adapter->hw_ready &&
	    !test_bit(__E1000_DOWN, &adapter->state))
		e1000_watchdog_task(adapter);
	e1000_unlock_hw(adapter);

	InterlockedExchange(&adapter->watchdog_queued, 0);
}

void e1000_schedule_reset(struct e1000_adapter *adapter)
{
	if (adapter->halting)
		return;
	if (InterlockedCompareExchange(&adapter->reset_pending, 1, 0) == 0)
		NdisQueueIoWorkItem(adapter->reset_work, E1000ResetWork,
				    adapter);
}

VOID E1000ResetWork(PVOID WorkItemContext, NDIS_HANDLE NdisIoWorkItemHandle)
{
	struct e1000_adapter *adapter = WorkItemContext;

	e1000_lock_hw(adapter);
	if (!adapter->halting && adapter->power_state == NdisDeviceStateD0)
		e1000e_reinit_locked(adapter);
	e1000_unlock_hw(adapter);

	InterlockedExchange(&adapter->reset_pending, 0);

	if (InterlockedExchange(&adapter->ndis_reset_requested, 0))
		NdisMResetComplete(adapter->miniport, NDIS_STATUS_SUCCESS,
				   FALSE);
}

static void e1000_wait_for_work(struct e1000_adapter *adapter)
{
	KeFlushQueuedDpcs();

	while (adapter->watchdog_queued || adapter->reset_pending)
		msleep(10);
}

static void e1000_cleanup(struct e1000_adapter *adapter)
{
	if (adapter->watchdog_timer) {
		NdisCancelTimerObject(adapter->watchdog_timer);
		adapter->halting = true;
		e1000_wait_for_work(adapter);
		NdisFreeTimerObject(adapter->watchdog_timer);
	}
	adapter->halting = true;
	e1000_wait_for_work(adapter);

	if (adapter->interrupt_registered) {
		NdisMDeregisterInterruptEx(adapter->interrupt);
		adapter->interrupt_registered = false;
	}

	if (adapter->watchdog_work)
		NdisFreeIoWorkItem(adapter->watchdog_work);
	if (adapter->reset_work)
		NdisFreeIoWorkItem(adapter->reset_work);

	e1000_tx_complete_all_waiting(adapter, NDIS_STATUS_FAILURE, false);
	e1000_free_rings(adapter);

	if (adapter->dma)
		NdisMDeregisterScatterGatherDma(adapter->dma);

	if (adapter->hw.flash_address &&
	    adapter->hw.mac.type < e1000_pch_spt && adapter->flash_len)
		NdisMUnmapIoSpace(adapter->miniport, adapter->hw.flash_address,
				  adapter->flash_len);
	adapter->hw.flash_address = NULL;

	if (adapter->hw.hw_addr) {
		u8 *va = adapter->hw.hw_addr;

		adapter->hw.hw_addr = NULL;
		NdisMUnmapIoSpace(adapter->miniport, va, adapter->mmio_len);
	}

	NdisFreeSpinLock(&adapter->tx_lock);
	NdisFreeSpinLock(&adapter->rx_lock);
	NdisFreeSpinLock(&adapter->filter_lock);

	NdisFreeMemory(adapter, 0, 0);
}

static NDIS_STATUS e1000_map_resources(struct e1000_adapter *adapter,
				       PNDIS_RESOURCE_LIST res,
				       PCI_COMMON_CONFIG *cfg)
{
	ULONGLONG bar0 = e1000_bar_address(cfg, 0);
	ULONGLONG bar1 = e1000_bar_address(cfg, 1);
	PCM_PARTIAL_RESOURCE_DESCRIPTOR mem0 = NULL, mem1 = NULL, first = NULL;
	bool have_irq = false;
	NDIS_STATUS status;
	ULONG i;

	if (!res)
		return NDIS_STATUS_RESOURCE_CONFLICT;

	for (i = 0; i < res->Count; i++) {
		PCM_PARTIAL_RESOURCE_DESCRIPTOR d = &res->PartialDescriptors[i];

		switch (d->Type) {
		case CmResourceTypeMemory:
			if (!first)
				first = d;
			if ((ULONGLONG)d->u.Memory.Start.QuadPart == bar0)
				mem0 = d;
			else if (bar1 &&
				 (ULONGLONG)d->u.Memory.Start.QuadPart == bar1)
				mem1 = d;
			break;
		case CmResourceTypeInterrupt:
			have_irq = true;
			break;
		default:
			break;
		}
	}

	if (!mem0)
		mem0 = first;
	if (!mem0 || !have_irq) {
		e_err("missing memory or interrupt resource\n");
		return NDIS_STATUS_RESOURCE_CONFLICT;
	}

	adapter->mmio_pa = mem0->u.Memory.Start;
	adapter->mmio_len = mem0->u.Memory.Length;
	status = NdisMMapIoSpace((PVOID *)&adapter->hw.hw_addr,
				 adapter->miniport, adapter->mmio_pa,
				 adapter->mmio_len);
	if (status != NDIS_STATUS_SUCCESS) {
		adapter->hw.hw_addr = NULL;
		return status;
	}

	if ((adapter->flags & FLAG_HAS_FLASH) &&
	    adapter->hw.mac.type < e1000_pch_spt) {
		if (!mem1) {
			e_err("flash BAR missing\n");
			return NDIS_STATUS_RESOURCE_CONFLICT;
		}
		adapter->flash_len = mem1->u.Memory.Length;
		status = NdisMMapIoSpace((PVOID *)&adapter->hw.flash_address,
					 adapter->miniport,
					 mem1->u.Memory.Start,
					 adapter->flash_len);
		if (status != NDIS_STATUS_SUCCESS) {
			adapter->hw.flash_address = NULL;
			adapter->flash_len = 0;
			return status;
		}
	}

	return NDIS_STATUS_SUCCESS;
}

static NDIS_STATUS e1000_register_interrupt(struct e1000_adapter *adapter)
{
	NDIS_MINIPORT_INTERRUPT_CHARACTERISTICS ic;
	NDIS_STATUS status;

	NdisZeroMemory(&ic, sizeof(ic));
	ic.Header.Type = NDIS_OBJECT_TYPE_MINIPORT_INTERRUPT;
	ic.Header.Revision = NDIS_MINIPORT_INTERRUPT_REVISION_1;
	ic.Header.Size = NDIS_SIZEOF_MINIPORT_INTERRUPT_CHARACTERISTICS_REVISION_1;
	ic.InterruptHandler = E1000Isr;
	ic.InterruptDpcHandler = E1000InterruptDpc;
	ic.DisableInterruptHandler = E1000DisableInterrupt;
	ic.EnableInterruptHandler = E1000EnableInterrupt;

	ic.MsiSupported = TRUE;
	ic.MsiSyncWithAllMessages = TRUE;
	ic.MessageInterruptHandler = E1000MsiIsr;
	ic.MessageInterruptDpcHandler = E1000MsiDpc;
	ic.DisableMessageInterruptHandler = E1000MsiDisable;
	ic.EnableMessageInterruptHandler = E1000MsiEnable;

	status = NdisMRegisterInterruptEx(adapter->miniport, adapter, &ic,
					  &adapter->interrupt);
	if (status == NDIS_STATUS_SUCCESS) {
		adapter->interrupt_registered = true;
		adapter->int_mode = ic.InterruptType == NDIS_CONNECT_MESSAGE_BASED;
		e_info("using %s interrupts\n", adapter->int_mode ? "MSI" : "INTx");
	}
	return status;
}

static void e1000_disable_msi_in_registry(struct e1000_adapter *adapter)
{
	PDEVICE_OBJECT pdo = NULL, fdo = NULL, next = NULL;
	OBJECT_ATTRIBUTES oa;
	UNICODE_STRING name;
	HANDLE dev_key = NULL, im_key = NULL, msi_key = NULL;
	ULONG zero = 0;
	NTSTATUS st;

	NdisMGetDeviceProperty(adapter->miniport, &pdo, &fdo, &next, NULL, NULL);
	if (!pdo)
		return;

	st = IoOpenDeviceRegistryKey(pdo, PLUGPLAY_REGKEY_DEVICE,
				     KEY_ALL_ACCESS, &dev_key);
	if (!NT_SUCCESS(st))
		return;

	RtlInitUnicodeString(&name, L"Interrupt Management");
	InitializeObjectAttributes(&oa, &name, OBJ_KERNEL_HANDLE, dev_key, NULL);
	st = ZwCreateKey(&im_key, KEY_ALL_ACCESS, &oa, 0, NULL, 0, NULL);
	if (NT_SUCCESS(st)) {
		RtlInitUnicodeString(&name, L"MessageSignaledInterruptProperties");
		InitializeObjectAttributes(&oa, &name, OBJ_KERNEL_HANDLE, im_key,
					   NULL);
		st = ZwCreateKey(&msi_key, KEY_ALL_ACCESS, &oa, 0, NULL, 0, NULL);
		if (NT_SUCCESS(st)) {
			RtlInitUnicodeString(&name, L"MSISupported");
			ZwSetValueKey(msi_key, &name, 0, REG_DWORD, &zero,
				      sizeof(zero));
			ZwClose(msi_key);
		}
		ZwClose(im_key);
	}
	ZwClose(dev_key);
}

static bool e1000_test_msi(struct e1000_adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	u16 pci_cmd = 0;
	int i;

	pci_read_config_word(adapter->pdev, 0x04, &pci_cmd);
	if (pci_cmd & 0x0100)
		pci_write_config_word(adapter->pdev, 0x04, pci_cmd & ~0x0100);

	er32(ICR);
	adapter->msi_test_ok = false;
	KeMemoryBarrier();

	e1000_irq_enable(adapter);

	ew32(ICS, E1000_ICS_RXSEQ);
	e1e_flush();
	for (i = 0; i < 10 && !adapter->msi_test_ok; i++)
		msleep(10);

	e1000_irq_disable(adapter);
	er32(ICR);

	if (pci_cmd & 0x0100)
		pci_write_config_word(adapter->pdev, 0x04, pci_cmd);

	return adapter->msi_test_ok;
}

static NDIS_STATUS e1000_register_dma(struct e1000_adapter *adapter)
{
	NDIS_SG_DMA_DESCRIPTION dd;

	NdisZeroMemory(&dd, sizeof(dd));
	dd.Header.Type = NDIS_OBJECT_TYPE_SG_DMA_DESCRIPTION;
	dd.Header.Revision = NDIS_SG_DMA_DESCRIPTION_REVISION_1;
	dd.Header.Size = NDIS_SIZEOF_SG_DMA_DESCRIPTION_REVISION_1;
	dd.Flags = NDIS_SG_DMA_64_BIT_ADDRESS;
	dd.MaximumPhysicalMapping = E1000_TX_BUFFER_SIZE;
	dd.ProcessSGListHandler = E1000ProcessSGList;
	dd.SharedMemAllocateCompleteHandler = NULL;

	return NdisMRegisterScatterGatherDma(adapter->miniport, &dd,
					     &adapter->dma);
}

static NDIS_STATUS e1000_set_registration_attributes(struct e1000_adapter *adapter)
{
	NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES ra;

	NdisZeroMemory(&ra, sizeof(ra));
	ra.Header.Type = NDIS_OBJECT_TYPE_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES;
	ra.Header.Revision = NDIS_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES_REVISION_1;
	ra.Header.Size = NDIS_SIZEOF_MINIPORT_ADAPTER_REGISTRATION_ATTRIBUTES_REVISION_1;
	ra.MiniportAdapterContext = adapter;
	ra.AttributeFlags = NDIS_MINIPORT_ATTRIBUTES_HARDWARE_DEVICE |
			    NDIS_MINIPORT_ATTRIBUTES_BUS_MASTER;
	ra.CheckForHangTimeInSeconds = 4;
	ra.InterfaceType = NdisInterfacePci;

	return NdisMSetMiniportAttributes(adapter->miniport,
			(PNDIS_MINIPORT_ADAPTER_ATTRIBUTES)&ra);
}

static NDIS_STATUS e1000_set_offload_attributes(struct e1000_adapter *adapter)
{
	NDIS_MINIPORT_ADAPTER_OFFLOAD_ATTRIBUTES oa;
	NDIS_OFFLOAD current, hardware;

	e1000_fill_offload(adapter, &current, false);
	e1000_fill_offload(adapter, &hardware, true);

	NdisZeroMemory(&oa, sizeof(oa));
	oa.Header.Type = NDIS_OBJECT_TYPE_MINIPORT_ADAPTER_OFFLOAD_ATTRIBUTES;
	oa.Header.Revision = NDIS_MINIPORT_ADAPTER_OFFLOAD_ATTRIBUTES_REVISION_1;
	oa.Header.Size = NDIS_SIZEOF_MINIPORT_ADAPTER_OFFLOAD_ATTRIBUTES_REVISION_1;
	oa.DefaultOffloadConfiguration = &current;
	oa.HardwareOffloadCapabilities = &hardware;

	return NdisMSetMiniportAttributes(adapter->miniport,
			(PNDIS_MINIPORT_ADAPTER_ATTRIBUTES)&oa);
}

static NDIS_STATUS e1000_set_general_attributes(struct e1000_adapter *adapter)
{
	NDIS_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES ga;
	NDIS_PNP_CAPABILITIES pm;

	e1000_fill_pm_caps(adapter, &pm);

	NdisZeroMemory(&ga, sizeof(ga));
	ga.Header.Type = NDIS_OBJECT_TYPE_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES;
	ga.Header.Revision = NDIS_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES_REVISION_1;
	ga.Header.Size = NDIS_SIZEOF_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES_REVISION_1;

	ga.MediaType = NdisMedium802_3;
	ga.PhysicalMediumType = NdisPhysicalMedium802_3;
	ga.MtuSize = adapter->mtu;
	ga.MaxXmitLinkSpeed = 1000000000ULL;
	ga.MaxRcvLinkSpeed = 1000000000ULL;
	ga.XmitLinkSpeed = NDIS_LINK_SPEED_UNKNOWN;
	ga.RcvLinkSpeed = NDIS_LINK_SPEED_UNKNOWN;
	ga.MediaConnectState = MediaConnectStateUnknown;
	ga.MediaDuplexState = MediaDuplexStateUnknown;
	ga.LookaheadSize = adapter->mtu;
	ga.PowerManagementCapabilities = &pm;
	ga.MacOptions = NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA |
			NDIS_MAC_OPTION_TRANSFERS_NOT_PEND |
			NDIS_MAC_OPTION_NO_LOOPBACK;
	if (adapter->cfg.priority_vlan & 1)
		ga.MacOptions |= NDIS_MAC_OPTION_8021P_PRIORITY;
	if (adapter->cfg.priority_vlan & 2)
		ga.MacOptions |= NDIS_MAC_OPTION_8021Q_VLAN;
	ga.SupportedPacketFilters = NDIS_PACKET_TYPE_DIRECTED |
				    NDIS_PACKET_TYPE_MULTICAST |
				    NDIS_PACKET_TYPE_ALL_MULTICAST |
				    NDIS_PACKET_TYPE_BROADCAST |
				    NDIS_PACKET_TYPE_PROMISCUOUS;
	ga.MaxMulticastListSize = E1000_MAX_MCAST_LIST;
	ga.MacAddressLength = ETH_ALEN;
	NdisMoveMemory(ga.PermanentMacAddress, adapter->perm_addr, ETH_ALEN);
	NdisMoveMemory(ga.CurrentMacAddress, adapter->curr_addr, ETH_ALEN);
	ga.RecvScaleCapabilities = NULL;
	ga.AccessType = NET_IF_ACCESS_BROADCAST;
	ga.DirectionType = NET_IF_DIRECTION_SENDRECEIVE;
	ga.ConnectionType = NET_IF_CONNECTION_DEDICATED;
	ga.IfType = IF_TYPE_ETHERNET_CSMACD;
	ga.IfConnectorPresent = TRUE;
	ga.SupportedStatistics = NDIS_STATISTICS_XMIT_OK_SUPPORTED |
				 NDIS_STATISTICS_RCV_OK_SUPPORTED |
				 NDIS_STATISTICS_XMIT_ERROR_SUPPORTED |
				 NDIS_STATISTICS_RCV_ERROR_SUPPORTED |
				 NDIS_STATISTICS_RCV_NO_BUFFER_SUPPORTED |
				 NDIS_STATISTICS_DIRECTED_BYTES_XMIT_SUPPORTED |
				 NDIS_STATISTICS_DIRECTED_FRAMES_XMIT_SUPPORTED |
				 NDIS_STATISTICS_MULTICAST_BYTES_XMIT_SUPPORTED |
				 NDIS_STATISTICS_MULTICAST_FRAMES_XMIT_SUPPORTED |
				 NDIS_STATISTICS_BROADCAST_BYTES_XMIT_SUPPORTED |
				 NDIS_STATISTICS_BROADCAST_FRAMES_XMIT_SUPPORTED |
				 NDIS_STATISTICS_DIRECTED_BYTES_RCV_SUPPORTED |
				 NDIS_STATISTICS_DIRECTED_FRAMES_RCV_SUPPORTED |
				 NDIS_STATISTICS_MULTICAST_BYTES_RCV_SUPPORTED |
				 NDIS_STATISTICS_MULTICAST_FRAMES_RCV_SUPPORTED |
				 NDIS_STATISTICS_BROADCAST_BYTES_RCV_SUPPORTED |
				 NDIS_STATISTICS_BROADCAST_FRAMES_RCV_SUPPORTED |
				 NDIS_STATISTICS_RCV_CRC_ERROR_SUPPORTED |
				 NDIS_STATISTICS_BYTES_RCV_SUPPORTED |
				 NDIS_STATISTICS_BYTES_XMIT_SUPPORTED |
				 NDIS_STATISTICS_RCV_DISCARDS_SUPPORTED |
				 NDIS_STATISTICS_GEN_STATISTICS_SUPPORTED |
				 NDIS_STATISTICS_XMIT_DISCARDS_SUPPORTED;
	ga.SupportedPauseFunctions = NdisPauseFunctionsSendAndReceive;
	ga.DataBackFillSize = 0;
	ga.ContextBackFillSize = 0;
	ga.SupportedOidList = e1000_supported_oids;
	ga.SupportedOidListLength = e1000_supported_oids_size;
	ga.AutoNegotiationFlags = NDIS_LINK_STATE_XMIT_LINK_SPEED_AUTO_NEGOTIATED |
				  NDIS_LINK_STATE_RCV_LINK_SPEED_AUTO_NEGOTIATED |
				  NDIS_LINK_STATE_DUPLEX_AUTO_NEGOTIATED |
				  NDIS_LINK_STATE_PAUSE_FUNCTIONS_AUTO_NEGOTIATED;

	return NdisMSetMiniportAttributes(adapter->miniport,
			(PNDIS_MINIPORT_ADAPTER_ATTRIBUTES)&ga);
}

NDIS_STATUS E1000Initialize(NDIS_HANDLE MiniportAdapterHandle,
			    NDIS_HANDLE MiniportDriverContext,
			    PNDIS_MINIPORT_INIT_PARAMETERS MiniportInitParameters)
{
	struct e1000_adapter *adapter;
	struct e1000_hw *hw;
	PCI_COMMON_CONFIG cfg;
	struct e1000_info *ei = NULL;
	NDIS_TIMER_CHARACTERISTICS tc;
	NDIS_STATUS status;
	LARGE_INTEGER due;
	ULONG i;

	adapter = NdisAllocateMemoryWithTagPriority(MiniportAdapterHandle,
						    sizeof(*adapter),
						    E1000EV_POOL_TAG,
						    NormalPoolPriority);
	if (!adapter)
		return NDIS_STATUS_RESOURCES;

	NdisZeroMemory(adapter, sizeof(*adapter));
	hw = &adapter->hw;
	adapter->miniport = MiniportAdapterHandle;
	adapter->hw.adapter = adapter;
	adapter->pdev = &adapter->pci;
	adapter->pci.miniport = MiniportAdapterHandle;
	adapter->ndis_state = E1000_NDIS_PAUSED;
	adapter->power_state = NdisDeviceStateD0;
	adapter->state = 0;
	set_bit(__E1000_DOWN, &adapter->state);
	NdisAllocateSpinLock(&adapter->tx_lock);
	NdisAllocateSpinLock(&adapter->rx_lock);
	NdisAllocateSpinLock(&adapter->filter_lock);
	KeInitializeMutex(&adapter->hw_mutex, 0);

	status = e1000_set_registration_attributes(adapter);
	if (status != NDIS_STATUS_SUCCESS)
		goto err;

	NdisZeroMemory(&cfg, sizeof(cfg));
	if (NdisMGetBusData(MiniportAdapterHandle, PCI_WHICHSPACE_CONFIG, 0,
			    &cfg, PCI_COMMON_HDR_LENGTH) != PCI_COMMON_HDR_LENGTH) {
		status = NDIS_STATUS_ADAPTER_NOT_FOUND;
		goto err;
	}

	if (cfg.VendorID == E1000_PCI_VENDOR_INTEL) {
		for (i = 0; i < ARRAY_SIZE(e1000_pci_tbl); i++) {
			if (e1000_pci_tbl[i].device == cfg.DeviceID) {
				ei = e1000_pci_tbl[i].info;
				break;
			}
		}
	}
	if (!ei) {
		status = NDIS_STATUS_ADAPTER_NOT_FOUND;
		goto err;
	}

	adapter->pci.vendor = cfg.VendorID;
	adapter->pci.device = cfg.DeviceID;
	adapter->pci.revision = cfg.RevisionID;
	adapter->pci.subsystem_vendor = cfg.u.type0.SubVendorID;
	adapter->pci.subsystem_device = cfg.u.type0.SubSystemID;
	adapter->pci.pcie_cap = e1000_find_pcie_cap(adapter, &cfg);

	if ((cfg.Command & (E1000_PCI_CMD_MEMORY | E1000_PCI_CMD_MASTER)) !=
	    (E1000_PCI_CMD_MEMORY | E1000_PCI_CMD_MASTER)) {
		u16 cmd = cfg.Command | E1000_PCI_CMD_MEMORY |
			  E1000_PCI_CMD_MASTER;

		pci_write_config_word(adapter->pdev, 0x04, cmd);
	}

	adapter->ei = ei;
	adapter->pba = ei->pba;
	adapter->flags = ei->flags;
	adapter->flags2 = ei->flags2;
	adapter->hw.mac.type = ei->mac;
	adapter->max_hw_frame_size = ei->max_hw_frame_size;

	status = e1000_map_resources(adapter,
				     MiniportInitParameters->AllocatedResources,
				     &cfg);
	if (status != NDIS_STATUS_SUCCESS)
		goto err;

	status = e1000_register_dma(adapter);
	if (status != NDIS_STATUS_SUCCESS)
		goto err;

	e1000_read_registry(adapter);

	e1000_lock_hw(adapter);
	if (e1000_hw_probe(adapter)) {
		e1000_unlock_hw(adapter);
		status = NDIS_STATUS_ADAPTER_NOT_FOUND;
		goto err;
	}

	if (is_zero_ether_addr(adapter->curr_addr))
		NdisMoveMemory(adapter->curr_addr, adapter->perm_addr, ETH_ALEN);
	NdisMoveMemory(hw->mac.addr, adapter->curr_addr, ETH_ALEN);

	e1000_init_mtu(adapter);
	e1000_init_offloads(adapter);
	e1000_apply_link_config(adapter);

	e1000e_reset(adapter);
	e1000_unlock_hw(adapter);

	status = e1000_alloc_rings(adapter);
	if (status != NDIS_STATUS_SUCCESS)
		goto err;

	adapter->watchdog_work = NdisAllocateIoWorkItem(MiniportAdapterHandle);
	adapter->reset_work = NdisAllocateIoWorkItem(MiniportAdapterHandle);
	if (!adapter->watchdog_work || !adapter->reset_work) {
		status = NDIS_STATUS_RESOURCES;
		goto err;
	}

	NdisZeroMemory(&tc, sizeof(tc));
	tc.Header.Type = NDIS_OBJECT_TYPE_TIMER_CHARACTERISTICS;
	tc.Header.Revision = NDIS_TIMER_CHARACTERISTICS_REVISION_1;
	tc.Header.Size = NDIS_SIZEOF_TIMER_CHARACTERISTICS_REVISION_1;
	tc.AllocationTag = E1000EV_POOL_TAG;
	tc.TimerFunction = E1000WatchdogTimer;
	tc.FunctionContext = adapter;
	status = NdisAllocateTimerObject(MiniportAdapterHandle, &tc,
					 &adapter->watchdog_timer);
	if (status != NDIS_STATUS_SUCCESS) {
		adapter->watchdog_timer = NULL;
		goto err;
	}

	status = e1000_register_interrupt(adapter);
	if (status != NDIS_STATUS_SUCCESS)
		goto err;

	if (adapter->int_mode) {
		bool ok;

		e1000_lock_hw(adapter);
		ok = e1000_test_msi(adapter);
		e1000_unlock_hw(adapter);
		if (!ok) {
			e_err("MSI interrupt test failed; INTx will be used after the adapter is restarted\n");
			e1000_disable_msi_in_registry(adapter);
			NdisWriteErrorLogEntry(MiniportAdapterHandle,
					       NDIS_ERROR_CODE_INTERRUPT_CONNECT,
					       1, 0x4D534921);
			status = NDIS_STATUS_FAILURE;
			goto err;
		}
	}

	status = e1000_set_general_attributes(adapter);
	if (status != NDIS_STATUS_SUCCESS)
		goto err;

	status = e1000_set_offload_attributes(adapter);
	if (status != NDIS_STATUS_SUCCESS)
		goto err;

	e1000_lock_hw(adapter);

	if (adapter->flags & FLAG_HAS_AMT) {
		e1000e_get_hw_control(adapter);
		e1000e_reset(adapter);
	}

	e1000e_power_up_phy(adapter);
	e1000e_up(adapter);
	e1000_unlock_hw(adapter);

	due.QuadPart = -(LONGLONG)E1000_WATCHDOG_MS * 10000;
	NdisSetTimerObject(adapter->watchdog_timer, due, E1000_WATCHDOG_MS,
			   adapter);

	return NDIS_STATUS_SUCCESS;

err:
	e_err("initialization failed: 0x%08x\n", status);
	if (adapter->hw_ready || !test_bit(__E1000_DOWN, &adapter->state)) {
		e1000_lock_hw(adapter);
		e1000e_down(adapter, true);
		e1000_unlock_hw(adapter);
	}
	if (adapter->hw.hw_addr && adapter->ei)
		e1000e_release_hw_control(adapter);
	e1000_cleanup(adapter);
	return status;
}

VOID E1000Halt(NDIS_HANDLE MiniportAdapterContext, NDIS_HALT_ACTION HaltAction)
{
	struct e1000_adapter *adapter = MiniportAdapterContext;

	adapter->halting = true;
	NdisCancelTimerObject(adapter->watchdog_timer);
	e1000_wait_for_work(adapter);

	e1000_lock_hw(adapter);
	if (!adapter->surprise_removed &&
	    adapter->power_state == NdisDeviceStateD0) {
		e1000e_down(adapter, true);

		if (adapter->flags & FLAG_HAS_AMT)
			e1000e_release_hw_control(adapter);
	} else {
		set_bit(__E1000_DOWN, &adapter->state);
		adapter->hw_ready = false;
	}
	e1000_unlock_hw(adapter);

	e1000_cleanup(adapter);
}

VOID E1000Unload(PDRIVER_OBJECT DriverObject)
{
	NdisMDeregisterMiniportDriver(e1000_driver_handle);
}

NDIS_STATUS E1000Pause(NDIS_HANDLE MiniportAdapterContext,
		       PNDIS_MINIPORT_PAUSE_PARAMETERS PauseParameters)
{
	struct e1000_adapter *adapter = MiniportAdapterContext;
	NDIS_STATUS status;

	NdisAcquireSpinLock(&adapter->rx_lock);
	adapter->ndis_state = E1000_NDIS_PAUSING;
	NdisReleaseSpinLock(&adapter->rx_lock);

	e1000_tx_complete_all_waiting(adapter, NDIS_STATUS_PAUSED, false);

	NdisAcquireSpinLock(&adapter->rx_lock);
	if (adapter->rx_outstanding == 0) {
		adapter->ndis_state = E1000_NDIS_PAUSED;
		status = NDIS_STATUS_SUCCESS;
	} else {
		status = NDIS_STATUS_PENDING;
	}
	NdisReleaseSpinLock(&adapter->rx_lock);

	return status;
}

NDIS_STATUS E1000Restart(NDIS_HANDLE MiniportAdapterContext,
			 PNDIS_MINIPORT_RESTART_PARAMETERS RestartParameters)
{
	struct e1000_adapter *adapter = MiniportAdapterContext;

	NdisAcquireSpinLock(&adapter->rx_lock);
	adapter->ndis_state = E1000_NDIS_RUNNING;
	NdisReleaseSpinLock(&adapter->rx_lock);

	e1000_schedule_watchdog(adapter);
	return NDIS_STATUS_SUCCESS;
}

static u32 e1000_wake_filters(struct e1000_adapter *adapter)
{
	if (!(adapter->flags & FLAG_HAS_WOL) || !adapter->cfg.wake_magic)
		return 0;
	if (adapter->wake_flags & NDIS_PNP_WAKE_UP_MAGIC_PACKET)
		return E1000_WUFC_MAG;
	return 0;
}

NDIS_STATUS e1000_set_power(struct e1000_adapter *adapter,
			    NDIS_DEVICE_POWER_STATE state)
{
	u16 cmd;

	e1000_lock_hw(adapter);

	if (state == NdisDeviceStateD0) {
		if (adapter->power_state != NdisDeviceStateD0) {
			pci_read_config_word(adapter->pdev, 0x04, &cmd);
			pci_write_config_word(adapter->pdev, 0x04,
					      cmd | E1000_PCI_CMD_MEMORY |
					      E1000_PCI_CMD_MASTER);

			adapter->power_state = NdisDeviceStateD0;
			e1000_resume(adapter);
			e1000e_up(adapter);
			adapter->link_indicated = false;
		}
	} else if (adapter->power_state == NdisDeviceStateD0) {
		e1000e_down(adapter, false);
		adapter->link_up = false;
		adapter->link_speed = 0;
		adapter->link_duplex = 0;
		e1000_suspend(adapter, e1000_wake_filters(adapter));
		adapter->power_state = state;
	}

	e1000_unlock_hw(adapter);

	if (state == NdisDeviceStateD0)
		e1000_schedule_watchdog(adapter);
	return NDIS_STATUS_SUCCESS;
}

BOOLEAN E1000CheckForHang(NDIS_HANDLE MiniportAdapterContext)
{
	return FALSE;
}

NDIS_STATUS E1000Reset(NDIS_HANDLE MiniportAdapterContext,
		       PBOOLEAN AddressingReset)
{
	struct e1000_adapter *adapter = MiniportAdapterContext;

	*AddressingReset = FALSE;
	InterlockedExchange(&adapter->ndis_reset_requested, 1);
	e1000_schedule_reset(adapter);
	return NDIS_STATUS_PENDING;
}

VOID E1000DevicePnPEvent(NDIS_HANDLE MiniportAdapterContext,
			 PNET_DEVICE_PNP_EVENT NetDevicePnPEvent)
{
	struct e1000_adapter *adapter = MiniportAdapterContext;

	if (NetDevicePnPEvent->DevicePnPEvent == NdisDevicePnPEventSurpriseRemoved) {
		adapter->surprise_removed = true;
		set_bit(__E1000_DOWN, &adapter->state);
	}
}

VOID E1000Shutdown(NDIS_HANDLE MiniportAdapterContext,
		   NDIS_SHUTDOWN_ACTION ShutdownAction)
{
	struct e1000_adapter *adapter = MiniportAdapterContext;
	struct e1000_hw *hw = &adapter->hw;

	if (!hw->hw_addr || adapter->surprise_removed ||
	    adapter->power_state != NdisDeviceStateD0)
		return;

	if (ShutdownAction == NdisShutdownPowerOff &&
	    KeGetCurrentIrql() == PASSIVE_LEVEL &&
	    adapter->cfg.wake_s5 && (adapter->flags & FLAG_HAS_WOL)) {
		LARGE_INTEGER timeout;

		timeout.QuadPart = -10000000LL;
		if (KeWaitForSingleObject(&adapter->hw_mutex, Executive,
					  KernelMode, FALSE, &timeout) ==
		    STATUS_SUCCESS) {
			adapter->halting = true;
			e1000e_down(adapter, false);
			e1000_suspend(adapter, E1000_WUFC_MAG);
			adapter->power_state = NdisDeviceStateD3;
			KeReleaseMutex(&adapter->hw_mutex, FALSE);
			return;
		}
	}

	set_bit(__E1000_DOWN, &adapter->state);
	writel(~0U, hw->hw_addr + E1000_IMC);
	writel(readl(hw->hw_addr + E1000_RCTL) & ~E1000_RCTL_EN,
	       hw->hw_addr + E1000_RCTL);
	writel(readl(hw->hw_addr + E1000_TCTL) & ~E1000_TCTL_EN,
	       hw->hw_addr + E1000_TCTL);
	readl(hw->hw_addr + E1000_STATUS);

	writel(readl(hw->hw_addr + E1000_CTRL_EXT) & ~E1000_CTRL_EXT_DRV_LOAD,
	       hw->hw_addr + E1000_CTRL_EXT);
}

VOID E1000ProcessSGList(PDEVICE_OBJECT DeviceObject, PVOID Reserved,
			PSCATTER_GATHER_LIST ScatterGatherListBuffer,
			PVOID Context)
{
}

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	NDIS_MINIPORT_DRIVER_CHARACTERISTICS mc;
	NDIS_STATUS status;

	e1000_ich8lan_init_tables();

	NdisZeroMemory(&mc, sizeof(mc));
	mc.Header.Type = NDIS_OBJECT_TYPE_MINIPORT_DRIVER_CHARACTERISTICS;
	mc.Header.Revision = NDIS_MINIPORT_DRIVER_CHARACTERISTICS_REVISION_1;
	mc.Header.Size = NDIS_SIZEOF_MINIPORT_DRIVER_CHARACTERISTICS_REVISION_1;
	mc.MajorNdisVersion = 6;
	mc.MinorNdisVersion = 0;
	mc.MajorDriverVersion = E1000EV_DRIVER_VERSION_MAJOR;
	mc.MinorDriverVersion = E1000EV_DRIVER_VERSION_MINOR;

	mc.InitializeHandlerEx = E1000Initialize;
	mc.HaltHandlerEx = E1000Halt;
	mc.UnloadHandler = E1000Unload;
	mc.PauseHandler = E1000Pause;
	mc.RestartHandler = E1000Restart;
	mc.OidRequestHandler = E1000OidRequest;
	mc.SendNetBufferListsHandler = E1000SendNetBufferLists;
	mc.ReturnNetBufferListsHandler = E1000ReturnNetBufferLists;
	mc.CancelSendHandler = E1000CancelSend;
	mc.CheckForHangHandlerEx = E1000CheckForHang;
	mc.ResetHandlerEx = E1000Reset;
	mc.DevicePnPEventNotifyHandler = E1000DevicePnPEvent;
	mc.ShutdownHandlerEx = E1000Shutdown;
	mc.CancelOidRequestHandler = E1000CancelOidRequest;

	status = NdisMRegisterMiniportDriver(DriverObject, RegistryPath, NULL,
					     &mc, &e1000_driver_handle);
	return status;
}
