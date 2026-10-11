// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 1999 - 2018 Intel Corporation. */

#include "e1000.h"

static void __ew32_prepare(struct e1000_hw *hw)
{
	s32 i = E1000_ICH_FWSM_PCIM2PCI_COUNT;

	while ((er32(FWSM) & E1000_ICH_FWSM_PCIM2PCI) && --i)
		udelay(50);
}

void __ew32(struct e1000_hw *hw, unsigned long reg, u32 val)
{
	if (hw->adapter->flags2 & FLAG2_PCIM2PCI_ARBITER_WA)
		__ew32_prepare(hw);

	writel(val, hw->hw_addr + reg);
}

void e1000_irq_disable(struct e1000_adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;

	ew32(IMC, ~0);
	e1e_flush();
}

void e1000_irq_enable(struct e1000_adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;

	if (hw->mac.type >= e1000_pch_lpt)
		ew32(IMS, IMS_ENABLE_MASK | E1000_IMS_ECCER);
	else
		ew32(IMS, IMS_ENABLE_MASK);
	e1e_flush();
}

void e1000e_get_hw_control(struct e1000_adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	u32 ctrl_ext;
	u32 swsm;

	if (adapter->flags & FLAG_HAS_SWSM_ON_LOAD) {
		swsm = er32(SWSM);
		ew32(SWSM, swsm | E1000_SWSM_DRV_LOAD);
	} else if (adapter->flags & FLAG_HAS_CTRLEXT_ON_LOAD) {
		ctrl_ext = er32(CTRL_EXT);
		ew32(CTRL_EXT, ctrl_ext | E1000_CTRL_EXT_DRV_LOAD);
	}
}

void e1000e_release_hw_control(struct e1000_adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	u32 ctrl_ext;
	u32 swsm;

	if (adapter->flags & FLAG_HAS_SWSM_ON_LOAD) {
		swsm = er32(SWSM);
		ew32(SWSM, swsm & ~E1000_SWSM_DRV_LOAD);
	} else if (adapter->flags & FLAG_HAS_CTRLEXT_ON_LOAD) {
		ctrl_ext = er32(CTRL_EXT);
		ew32(CTRL_EXT, ctrl_ext & ~E1000_CTRL_EXT_DRV_LOAD);
	}
}

void e1000e_write_itr(struct e1000_adapter *adapter, u32 itr)
{
	struct e1000_hw *hw = &adapter->hw;
	u32 new_itr = itr ? 1000000000 / (itr * 256) : 0;

	ew32(ITR, new_itr);
}

static unsigned int e1000_update_itr(u16 itr_setting, int packets, int bytes)
{
	unsigned int retval = itr_setting;

	if (packets == 0)
		return itr_setting;

	switch (itr_setting) {
	case lowest_latency:
		if (bytes / packets > 8000)
			retval = bulk_latency;
		else if ((packets < 5) && (bytes > 512))
			retval = low_latency;
		break;
	case low_latency:
		if (bytes > 10000) {
			if (bytes / packets > 8000)
				retval = bulk_latency;
			else if ((packets < 10) || ((bytes / packets) > 1200))
				retval = bulk_latency;
			else if ((packets > 35))
				retval = lowest_latency;
		} else if (bytes / packets > 2000) {
			retval = bulk_latency;
		} else if (packets <= 2 && bytes < 512) {
			retval = lowest_latency;
		}
		break;
	case bulk_latency:
		if (bytes > 25000) {
			if (packets > 35)
				retval = low_latency;
		} else if (bytes < 6000) {
			retval = low_latency;
		}
		break;
	}

	return retval;
}

void e1000_set_itr(struct e1000_adapter *adapter)
{
	u16 current_itr;
	u32 new_itr = adapter->itr;

	if (adapter->link_speed != SPEED_1000) {
		new_itr = 4000;
		goto set_itr_now;
	}

	if (adapter->flags2 & FLAG2_DISABLE_AIM) {
		new_itr = 0;
		goto set_itr_now;
	}

	adapter->tx_itr = (u16)e1000_update_itr(adapter->tx_itr,
					   adapter->total_tx_packets,
					   adapter->total_tx_bytes);
	if (adapter->itr_setting == 3 && adapter->tx_itr == lowest_latency)
		adapter->tx_itr = low_latency;

	adapter->rx_itr = (u16)e1000_update_itr(adapter->rx_itr,
					   adapter->total_rx_packets,
					   adapter->total_rx_bytes);
	if (adapter->itr_setting == 3 && adapter->rx_itr == lowest_latency)
		adapter->rx_itr = low_latency;

	current_itr = max(adapter->rx_itr, adapter->tx_itr);

	switch (current_itr) {
	case lowest_latency:
		new_itr = 70000;
		break;
	case low_latency:
		new_itr = 20000;
		break;
	case bulk_latency:
		new_itr = 4000;
		break;
	default:
		break;
	}

set_itr_now:
	if (new_itr != adapter->itr) {
		new_itr = new_itr > adapter->itr ?
		    min(adapter->itr + (new_itr >> 2), new_itr) : new_itr;
		adapter->itr = new_itr;
		e1000e_write_itr(adapter, new_itr);
	}
}

static void e1000_init_manageability_pt(struct e1000_adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	u32 manc, manc2h;

	if (!(adapter->flags & FLAG_MNG_PT_ENABLED))
		return;

	manc = er32(MANC);

	manc |= E1000_MANC_EN_MNG2HOST;
	manc2h = er32(MANC2H);
	manc2h |= (E1000_MANC2H_PORT_623 | E1000_MANC2H_PORT_664);

	ew32(MANC2H, manc2h);
	ew32(MANC, manc);
}

static void e1000_configure_tx(struct e1000_adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	struct e1000_ring *tx_ring = &adapter->tx_ring;
	u64 tdba;
	u32 tdlen, tctl;

	tdba = (u64)tx_ring->dma.QuadPart;
	tdlen = tx_ring->count * sizeof(struct e1000_tx_desc);
	ew32(TDBAL(0), (u32)(tdba & DMA_BIT_MASK(32)));
	ew32(TDBAH(0), (u32)(tdba >> 32));
	ew32(TDLEN(0), tdlen);
	ew32(TDH(0), 0);
	ew32(TDT(0), 0);
	tx_ring->head = adapter->hw.hw_addr + E1000_TDH(0);
	tx_ring->tail = adapter->hw.hw_addr + E1000_TDT(0);

	ew32(TIDV, adapter->tx_int_delay);
	ew32(TADV, adapter->tx_abs_int_delay);

	ew32(TXDCTL(1), er32(TXDCTL(0)));

	tctl = er32(TCTL);
	tctl &= ~E1000_TCTL_CT;
	tctl |= E1000_TCTL_PSP | E1000_TCTL_RTLC |
		(E1000_COLLISION_THRESHOLD << E1000_CT_SHIFT);

	adapter->txd_cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_IFCS;

	if (adapter->tx_int_delay)
		adapter->txd_cmd |= E1000_TXD_CMD_IDE;

	adapter->txd_cmd |= E1000_TXD_CMD_RS;

	ew32(TCTL, tctl);

	hw->mac.ops.config_collision_dist(hw);

	if (hw->mac.type == e1000_pch_spt) {
		u32 reg_val;

		reg_val = er32(IOSFPC);
		reg_val |= E1000_RCTL_RDMTS_HEX;
		ew32(IOSFPC, reg_val);

		reg_val = er32(TARC(0));
		reg_val &= ~E1000_TARC0_CB_MULTIQ_3_REQ;
		reg_val |= E1000_TARC0_CB_MULTIQ_2_REQ;
		ew32(TARC(0), reg_val);
	}
}

static void e1000_setup_rctl(struct e1000_adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	u32 rctl, rfctl;

	if (hw->mac.type >= e1000_pch2lan) {
		s32 ret_val;

		if (adapter->mtu > ETH_DATA_LEN)
			ret_val = e1000_lv_jumbo_workaround_ich8lan(hw, true);
		else
			ret_val = e1000_lv_jumbo_workaround_ich8lan(hw, false);

		if (ret_val)
			e_dbg("failed to enable|disable jumbo frame workaround mode\n");
	}

	rctl = er32(RCTL);
	rctl &= ~(3 << E1000_RCTL_MO_SHIFT);
	rctl |= E1000_RCTL_EN | E1000_RCTL_BAM |
	    E1000_RCTL_LBM_NO | E1000_RCTL_RDMTS_HALF |
	    (adapter->hw.mac.mc_filter_type << E1000_RCTL_MO_SHIFT);

	rctl &= ~E1000_RCTL_SBP;

	if (adapter->mtu <= ETH_DATA_LEN)
		rctl &= ~E1000_RCTL_LPE;
	else
		rctl |= E1000_RCTL_LPE;

	if (adapter->flags2 & FLAG2_CRC_STRIPPING)
		rctl |= E1000_RCTL_SECRC;

	rctl &= ~E1000_RCTL_SZ_4096;
	rctl |= E1000_RCTL_SZ_2048;
	rctl &= ~E1000_RCTL_BSEX;

	rfctl = er32(RFCTL);
	rfctl |= E1000_RFCTL_EXTEN;
	ew32(RFCTL, rfctl);

	rctl &= ~E1000_RCTL_DTYP_PS;

	ew32(RCTL, rctl);
	adapter->flags &= ~FLAG_RESTART_NOW;
}

static bool e1000_rx_csum_wanted(struct e1000_adapter *adapter)
{
	struct e1000_offload_state *o = &adapter->off;

	return o->rx_ip4 || o->rx_tcp4 || o->rx_udp4 || o->rx_tcp6 ||
	       o->rx_udp6;
}

static void e1000_configure_rx(struct e1000_adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	struct e1000_ring *rx_ring = &adapter->rx_ring;
	u64 rdba;
	u32 rdlen, rctl, rxcsum, ctrl_ext;

	rdlen = rx_ring->count * sizeof(union e1000_rx_desc_extended);

	rctl = er32(RCTL);
	if (!(adapter->flags2 & FLAG2_NO_DISABLE_RX))
		ew32(RCTL, rctl & ~E1000_RCTL_EN);
	e1e_flush();
	usleep_range(10000, 11000);

	ew32(RDTR, adapter->rx_int_delay);

	ew32(RADV, adapter->rx_abs_int_delay);
	if ((adapter->itr_setting != 0) && (adapter->itr != 0))
		e1000e_write_itr(adapter, adapter->itr);
	else
		e1000e_write_itr(adapter, 0);

	ctrl_ext = er32(CTRL_EXT);
	ctrl_ext |= E1000_CTRL_EXT_IAME;
	ew32(IAM, 0xffffffff);
	ew32(CTRL_EXT, ctrl_ext);
	e1e_flush();

	rdba = (u64)rx_ring->dma.QuadPart;
	ew32(RDBAL(0), (u32)(rdba & DMA_BIT_MASK(32)));
	ew32(RDBAH(0), (u32)(rdba >> 32));
	ew32(RDLEN(0), rdlen);
	ew32(RDH(0), 0);
	ew32(RDT(0), 0);
	rx_ring->head = adapter->hw.hw_addr + E1000_RDH(0);
	rx_ring->tail = adapter->hw.hw_addr + E1000_RDT(0);

	rxcsum = er32(RXCSUM);
	if (e1000_rx_csum_wanted(adapter))
		rxcsum |= E1000_RXCSUM_TUOFL | E1000_RXCSUM_IPOFL;
	else
		rxcsum &= ~(E1000_RXCSUM_TUOFL | E1000_RXCSUM_IPOFL);
	ew32(RXCSUM, rxcsum);

	if (adapter->mtu > ETH_DATA_LEN) {
		u32 rxdctl = er32(RXDCTL(0));

		ew32(RXDCTL(0), rxdctl | 0x3 | BIT(8));
	}

	ew32(RCTL, rctl);
}

void e1000e_set_rx_mode(struct e1000_adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	ULONG filter = adapter->packet_filter;
	u32 rctl, ctrl;
	u32 rar_entries;

	rctl = er32(RCTL);

	rctl &= ~(E1000_RCTL_UPE | E1000_RCTL_MPE | E1000_RCTL_BAM);

	if (filter & NDIS_PACKET_TYPE_PROMISCUOUS) {
		rctl |= (E1000_RCTL_UPE | E1000_RCTL_MPE | E1000_RCTL_BAM);
	} else {
		if (filter & NDIS_PACKET_TYPE_ALL_MULTICAST)
			rctl |= E1000_RCTL_MPE;
		if (filter & NDIS_PACKET_TYPE_BROADCAST)
			rctl |= E1000_RCTL_BAM;
	}

	if ((filter & (NDIS_PACKET_TYPE_MULTICAST |
		       NDIS_PACKET_TYPE_ALL_MULTICAST |
		       NDIS_PACKET_TYPE_PROMISCUOUS)) && adapter->mc_count)
		hw->mac.ops.update_mc_addr_list(hw, adapter->mc_list,
						adapter->mc_count);
	else
		hw->mac.ops.update_mc_addr_list(hw, NULL, 0);

	rar_entries = hw->mac.ops.rar_get_count(hw) - 1;
	for (; rar_entries > 0; rar_entries--) {
		ew32(RAH(rar_entries), 0);
		ew32(RAL(rar_entries), 0);
	}
	e1e_flush();

	ew32(RCTL, rctl);

	ctrl = er32(CTRL);
	if (adapter->cfg.priority_vlan)
		ctrl |= E1000_CTRL_VME;
	else
		ctrl &= ~E1000_CTRL_VME;
	ew32(CTRL, ctrl);
}

void e1000_configure(struct e1000_adapter *adapter)
{
	NdisAcquireSpinLock(&adapter->filter_lock);
	e1000e_set_rx_mode(adapter);
	NdisReleaseSpinLock(&adapter->filter_lock);

	e1000_init_manageability_pt(adapter);

	e1000_configure_tx(adapter);
	e1000_setup_rctl(adapter);
	e1000_configure_rx(adapter);
	e1000_alloc_rx_buffers(adapter);
}

void e1000e_power_up_phy(struct e1000_adapter *adapter)
{
	if (adapter->hw.phy.ops.power_up)
		adapter->hw.phy.ops.power_up(&adapter->hw);

	adapter->hw.mac.ops.setup_link(&adapter->hw);
}

void e1000_power_down_phy(struct e1000_adapter *adapter)
{
	if (adapter->hw.phy.ops.power_down)
		adapter->hw.phy.ops.power_down(&adapter->hw);
}

static void e1000_flush_tx_ring(struct e1000_adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	struct e1000_ring *tx_ring = &adapter->tx_ring;
	struct e1000_tx_desc *tx_desc = NULL;
	u32 tdt, tctl, txd_lower = E1000_TXD_CMD_IFCS;
	u16 size = 512;

	tctl = er32(TCTL);
	ew32(TCTL, tctl | E1000_TCTL_EN);
	tdt = er32(TDT(0));
	if (tdt != tx_ring->next_to_use)
		e_err("TDT %u != next_to_use %u while flushing\n",
		      tdt, tx_ring->next_to_use);
	tx_desc = E1000_TX_DESC(*tx_ring, tx_ring->next_to_use);
	tx_desc->buffer_addr = cpu_to_le64(tx_ring->dma.QuadPart);

	tx_desc->lower.data = cpu_to_le32(txd_lower | size);
	tx_desc->upper.data = 0;
	wmb();
	tx_ring->next_to_use++;
	if (tx_ring->next_to_use == tx_ring->count)
		tx_ring->next_to_use = 0;
	ew32(TDT(0), tx_ring->next_to_use);
	usleep_range(200, 250);
}

static void e1000_flush_rx_ring(struct e1000_adapter *adapter)
{
	u32 rctl, rxdctl;
	struct e1000_hw *hw = &adapter->hw;

	rctl = er32(RCTL);
	ew32(RCTL, rctl & ~E1000_RCTL_EN);
	e1e_flush();
	usleep_range(100, 150);

	rxdctl = er32(RXDCTL(0));
	rxdctl &= 0xffffc000;

	rxdctl |= (0x1F | BIT(8) | E1000_RXDCTL_THRESH_UNIT_DESC);

	ew32(RXDCTL(0), rxdctl);
	ew32(RCTL, rctl | E1000_RCTL_EN);
	e1e_flush();
	usleep_range(100, 150);
	ew32(RCTL, rctl & ~E1000_RCTL_EN);
}

static void e1000_flush_desc_rings(struct e1000_adapter *adapter)
{
	u16 hang_state;
	u32 fext_nvm11, tdlen;
	struct e1000_hw *hw = &adapter->hw;

	fext_nvm11 = er32(FEXTNVM11);
	fext_nvm11 |= E1000_FEXTNVM11_DISABLE_MULR_FIX;
	ew32(FEXTNVM11, fext_nvm11);
	tdlen = er32(TDLEN(0));
	pci_read_config_word(adapter->pdev, PCICFG_DESC_RING_STATUS,
			     &hang_state);
	if (!(hang_state & FLUSH_DESC_REQUIRED) || !tdlen)
		return;
	e1000_flush_tx_ring(adapter);
	pci_read_config_word(adapter->pdev, PCICFG_DESC_RING_STATUS,
			     &hang_state);
	if (hang_state & FLUSH_DESC_REQUIRED)
		e1000_flush_rx_ring(adapter);
}

void e1000e_reset(struct e1000_adapter *adapter)
{
	struct e1000_mac_info *mac = &adapter->hw.mac;
	struct e1000_fc_info *fc = &adapter->hw.fc;
	struct e1000_hw *hw = &adapter->hw;
	u32 tx_space, min_tx_space, min_rx_space;
	u32 pba = adapter->pba;

	ew32(PBA, pba);

	if (adapter->max_frame_size > (VLAN_ETH_FRAME_LEN + ETH_FCS_LEN)) {
		pba = er32(PBA);
		tx_space = pba >> 16;
		pba &= 0xffff;
		min_tx_space = (adapter->max_frame_size +
				sizeof(struct e1000_tx_desc) - ETH_FCS_LEN) * 2;
		min_tx_space = ALIGN(min_tx_space, 1024);
		min_tx_space >>= 10;
		min_rx_space = adapter->max_frame_size;
		min_rx_space = ALIGN(min_rx_space, 1024);
		min_rx_space >>= 10;

		if ((tx_space < min_tx_space) &&
		    ((min_tx_space - tx_space) < pba)) {
			pba -= min_tx_space - tx_space;

			if (pba < min_rx_space)
				pba = min_rx_space;
		}

		ew32(PBA, pba);
	}

	fc->pause_time = E1000_FC_PAUSE_TIME;
	fc->send_xon = true;
	fc->current_mode = fc->requested_mode;

	fc->refresh_time = 0xFFFF;
	fc->pause_time = 0xFFFF;

	if (adapter->mtu <= ETH_DATA_LEN) {
		fc->high_water = 0x05C20;
		fc->low_water = 0x05048;
	} else {
		pba = 14;
		ew32(PBA, pba);
		fc->high_water = ((pba << 10) * 9 / 10) & E1000_FCRTH_RTH;
		fc->low_water = ((pba << 10) * 8 / 10) & E1000_FCRTL_RTL;
	}

	adapter->tx_fifo_limit = min_t(u32, ((er32(PBA) >> 16) << 10) - 96,
				       24 << 10);

	if (adapter->itr_setting & 0x3) {
		if ((adapter->max_frame_size * 2) > (pba << 10)) {
			if (!(adapter->flags2 & FLAG2_DISABLE_AIM)) {
				e_info("Interrupt Throttle Rate off\n");
				adapter->flags2 |= FLAG2_DISABLE_AIM;
				adapter->itr = 0;
				e1000e_write_itr(adapter, 0);
			}
		} else if (adapter->flags2 & FLAG2_DISABLE_AIM) {
			e_info("Interrupt Throttle Rate on\n");
			adapter->flags2 &= ~FLAG2_DISABLE_AIM;
			adapter->itr = 20000;
			e1000e_write_itr(adapter, adapter->itr);
		}
	}

	if (hw->mac.type >= e1000_pch_spt)
		e1000_flush_desc_rings(adapter);
	mac->ops.reset_hw(hw);

	if (adapter->flags & FLAG_HAS_AMT)
		e1000e_get_hw_control(adapter);

	ew32(WUC, 0);

	if (mac->ops.init_hw(hw))
		e_err("Hardware Error\n");

	ew32(VET, ETH_P_8021Q);

	e1000e_reset_adaptive(hw);

	if (adapter->flags2 & FLAG2_HAS_EEE) {
		s32 ret_val;
		u16 adv_addr;

		switch (hw->phy.type) {
		case e1000_phy_82579:
			adv_addr = I82579_EEE_ADVERTISEMENT;
			break;
		case e1000_phy_i217:
			adv_addr = I217_EEE_ADVERTISEMENT;
			break;
		default:
			e_err("Invalid PHY type setting EEE advertisement\n");
			return;
		}

		ret_val = hw->phy.ops.acquire(hw);
		if (ret_val) {
			e_err("EEE advertisement - unable to acquire PHY\n");
			return;
		}

		e1000_write_emi_reg_locked(hw, adv_addr,
					   hw->dev_spec.ich8lan.eee_disable ?
					   0 : adapter->eee_advert);

		hw->phy.ops.release(hw);
	}

	e1000_get_phy_info(hw);

	if (hw->mac.type >= e1000_pch_spt && adapter->int_mode == 0) {
		u32 reg;

		reg = er32(FEXTNVM7);
		reg |= E1000_FEXTNVM7_SIDE_CLK_UNGATE;
		ew32(FEXTNVM7, reg);
		reg = er32(FEXTNVM9);
		reg |= E1000_FEXTNVM9_IOSFSB_CLKGATE_DIS |
		       E1000_FEXTNVM9_IOSFSB_CLKREQ_DIS;
		ew32(FEXTNVM9, reg);
	}
}

static void e1000e_trigger_lsc(struct e1000_adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;

	ew32(ICS, E1000_ICS_LSC);
}

void e1000e_up(struct e1000_adapter *adapter)
{
	e1000_configure(adapter);

	clear_bit(__E1000_DOWN, &adapter->state);
	adapter->hw_ready = true;

	NdisAcquireSpinLock(&adapter->filter_lock);
	e1000e_set_rx_mode(adapter);
	NdisReleaseSpinLock(&adapter->filter_lock);

	e1000_irq_enable(adapter);

	adapter->hw.mac.get_link_status = true;
	e1000e_trigger_lsc(adapter);
}

void e1000e_down(struct e1000_adapter *adapter, bool reset)
{
	struct e1000_hw *hw = &adapter->hw;
	u32 tctl, rctl;

	set_bit(__E1000_DOWN, &adapter->state);

	rctl = er32(RCTL);
	if (!(adapter->flags2 & FLAG2_NO_DISABLE_RX))
		ew32(RCTL, rctl & ~E1000_RCTL_EN);

	tctl = er32(TCTL);
	tctl &= ~E1000_TCTL_EN;
	ew32(TCTL, tctl);

	e1e_flush();
	usleep_range(10000, 11000);

	e1000_irq_disable(adapter);

	while (adapter->dpc_refs)
		msleep(1);

	adapter->hw_ready = false;

	e1000e_update_stats(adapter);

	adapter->link_speed = 0;
	adapter->link_duplex = 0;

	if ((hw->mac.type >= e1000_pch2lan) &&
	    (adapter->mtu > ETH_DATA_LEN) &&
	    e1000_lv_jumbo_workaround_ich8lan(hw, false))
		e_dbg("failed to disable jumbo frame workaround mode\n");

	if (reset)
		e1000e_reset(adapter);
	else if (hw->mac.type >= e1000_pch_spt)
		e1000_flush_desc_rings(adapter);

	e1000_clean_tx_ring(adapter);
	e1000_clean_rx_ring(adapter);
}

int e1000_sw_init(struct e1000_adapter *adapter)
{
	adapter->mtu = ETH_DATA_LEN;
	adapter->max_frame_size = adapter->mtu + VLAN_ETH_HLEN + ETH_FCS_LEN;
	adapter->min_frame_size = ETH_ZLEN + ETH_FCS_LEN;

	adapter->tx_int_delay = E1000_DEFAULT_TIDV;
	adapter->tx_abs_int_delay = E1000_DEFAULT_TADV;
	adapter->rx_int_delay = E1000_DEFAULT_RDTR;
	adapter->rx_abs_int_delay = E1000_DEFAULT_RADV;

	if (!adapter->cfg.int_moderation || adapter->cfg.itr == 0) {
		adapter->itr_setting = 0;
		adapter->itr = 0;
	} else if (adapter->cfg.itr == E1000_ITR_ADAPTIVE) {
		adapter->itr_setting = 3;
		adapter->itr = E1000_ITR_START;
	} else {
		adapter->itr_setting = adapter->cfg.itr & ~3;
		adapter->itr = adapter->cfg.itr;
	}
	adapter->tx_itr = low_latency;
	adapter->rx_itr = low_latency;

	adapter->int_mode = 0;
	adapter->flags2 |= FLAG2_CRC_STRIPPING | FLAG2_DFLT_CRC_STRIPPING;
	adapter->flags |= FLAG_READ_ONLY_NVM;

	e1000_irq_disable(adapter);

	set_bit(__E1000_DOWN, &adapter->state);
	return 0;
}

int e1000_hw_probe(struct e1000_adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	const struct e1000_info *ei = adapter->ei;
	u16 eeprom_data = 0;
	u16 eeprom_apme_mask = E1000_EEPROM_APME;
	int i;
	s32 err;

	if (adapter->flags2 & FLAG2_HAS_EEE)
		adapter->eee_advert = MDIO_EEE_100TX | MDIO_EEE_1000T;

	err = e1000_sw_init(adapter);
	if (err)
		return err;

	RtlCopyMemory(&hw->mac.ops, ei->mac_ops, sizeof(hw->mac.ops));
	RtlCopyMemory(&hw->nvm.ops, ei->nvm_ops, sizeof(hw->nvm.ops));
	RtlCopyMemory(&hw->phy.ops, ei->phy_ops, sizeof(hw->phy.ops));

	err = ei->get_variants(adapter);
	if (err) {
		e_err("get_variants failed: %d\n", err);
		return err;
	}

	if ((adapter->flags & FLAG_IS_ICH) &&
	    (adapter->flags & FLAG_READ_ONLY_NVM) &&
	    (hw->mac.type < e1000_pch_spt))
		e1000e_write_protect_nvm_ich8lan(&adapter->hw);

	hw->mac.ops.get_bus_info(&adapter->hw);

	adapter->hw.phy.autoneg_wait_to_complete = 0;

	if (adapter->hw.phy.media_type == e1000_media_type_copper) {
		adapter->hw.phy.mdix = AUTO_ALL_MODES;
		adapter->hw.phy.disable_polarity_correction = 0;
		adapter->hw.phy.ms_type = e1000_ms_hw_default;
	}

	if (hw->phy.ops.check_reset_block && hw->phy.ops.check_reset_block(hw))
		e_info("PHY reset is blocked due to SOL/IDER session.\n");

	if (e1000e_enable_mng_pass_thru(&adapter->hw))
		adapter->flags |= FLAG_MNG_PT_ENABLED;

	adapter->hw.mac.ops.reset_hw(&adapter->hw);

	for (i = 0;; i++) {
		if (e1000_validate_nvm_checksum(&adapter->hw) >= 0)
			break;
		if (i == 2) {
			e_err("The NVM Checksum Is Not Valid\n");
			goto err_eeprom;
		}
	}

	if (e1000e_read_mac_addr(&adapter->hw))
		e_err("NVM Read Error while reading MAC address\n");

	RtlCopyMemory(adapter->perm_addr, hw->mac.perm_addr, ETH_ALEN);

	if (!is_valid_ether_addr(adapter->perm_addr)) {
		e_err("Invalid MAC Address\n");
		goto err_eeprom;
	}

	adapter->hw.mac.autoneg = 1;
	adapter->hw.fc.requested_mode = e1000_fc_default;
	adapter->hw.fc.current_mode = e1000_fc_default;
	adapter->hw.phy.autoneg_advertised = 0x2f;

	if (adapter->flags & FLAG_APME_IN_WUC) {
		eeprom_data = (u16)er32(WUC);
		eeprom_apme_mask = E1000_WUC_APME;
		if ((hw->mac.type > e1000_ich10lan) &&
		    (er32(WUC) & E1000_WUC_PHY_WAKE))
			adapter->flags2 |= FLAG2_HAS_PHY_WAKEUP;
	}

	if (eeprom_data & eeprom_apme_mask)
		adapter->eeprom_wol |= E1000_WUFC_MAG;

	if (!(adapter->flags & FLAG_HAS_WOL))
		adapter->eeprom_wol = 0;

	adapter->mng_vlan_id = E1000_MNG_VLAN_NONE;

	return 0;

err_eeprom:
	if (hw->phy.ops.check_reset_block && !hw->phy.ops.check_reset_block(hw))
		e1000_phy_hw_reset(&adapter->hw);
	return -1;
}

void e1000_apply_link_config(struct e1000_adapter *adapter)
{
	struct e1000_mac_info *mac = &adapter->hw.mac;
	struct e1000_hw *hw = &adapter->hw;

	mac->autoneg = 1;
	hw->phy.autoneg_advertised = 0x2f;

	switch (adapter->cfg.speed_duplex) {
	case 1:
		mac->autoneg = 0;
		mac->forced_speed_duplex = ADVERTISE_10_HALF;
		break;
	case 2:
		mac->autoneg = 0;
		mac->forced_speed_duplex = ADVERTISE_10_FULL;
		break;
	case 3:
		mac->autoneg = 0;
		mac->forced_speed_duplex = ADVERTISE_100_HALF;
		break;
	case 4:
		mac->autoneg = 0;
		mac->forced_speed_duplex = ADVERTISE_100_FULL;
		break;
	case 6:
		hw->phy.autoneg_advertised = ADVERTISE_1000_FULL;
		break;
	default:
		break;
	}

	hw->phy.mdix = AUTO_ALL_MODES;

	switch (adapter->cfg.flow_control) {
	case 0:
		hw->fc.requested_mode = e1000_fc_none;
		break;
	case 1:
		hw->fc.requested_mode = e1000_fc_tx_pause;
		break;
	case 2:
		hw->fc.requested_mode = e1000_fc_rx_pause;
		break;
	default:
		hw->fc.requested_mode = e1000_fc_default;
		break;
	}
	hw->fc.current_mode = hw->fc.requested_mode;

	hw->dev_spec.ich8lan.eee_disable = !adapter->cfg.eee;
}

void e1000e_update_stats(struct e1000_adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;

	if (adapter->link_speed == 0)
		return;

	adapter->stats.crcerrs += er32(CRCERRS);
	adapter->stats.gprc += er32(GPRC);
	adapter->stats.gorc += er32(GORCL);
	er32(GORCH);
	adapter->stats.bprc += er32(BPRC);
	adapter->stats.mprc += er32(MPRC);
	adapter->stats.roc += er32(ROC);

	adapter->stats.mpc += er32(MPC);

	if (adapter->link_duplex == HALF_DUPLEX) {
		adapter->stats.scc += er32(SCC);
		adapter->stats.ecol += er32(ECOL);
		adapter->stats.mcc += er32(MCC);
		adapter->stats.latecol += er32(LATECOL);
		adapter->stats.dc += er32(DC);

		hw->mac.collision_delta = er32(COLC);
		adapter->stats.tncrs += er32(TNCRS);
		adapter->stats.colc += hw->mac.collision_delta;
	}

	adapter->stats.xonrxc += er32(XONRXC);
	adapter->stats.xontxc += er32(XONTXC);
	adapter->stats.xoffrxc += er32(XOFFRXC);
	adapter->stats.xofftxc += er32(XOFFTXC);
	adapter->stats.gptc += er32(GPTC);
	adapter->stats.gotc += er32(GOTCL);
	er32(GOTCH);
	adapter->stats.rnbc += er32(RNBC);
	adapter->stats.ruc += er32(RUC);

	adapter->stats.mptc += er32(MPTC);
	adapter->stats.bptc += er32(BPTC);

	hw->mac.tx_packet_delta = er32(TPT);
	adapter->stats.tpt += hw->mac.tx_packet_delta;

	adapter->stats.algnerrc += er32(ALGNERRC);
	adapter->stats.rxerrc += er32(RXERRC);
	adapter->stats.cexterr += er32(CEXTERR);
	adapter->stats.tsctc += er32(TSCTC);
	adapter->stats.tsctfc += er32(TSCTFC);

	adapter->stats.mgptc += er32(MGTPTC);
	adapter->stats.mgprc += er32(MGTPRC);
	adapter->stats.mgpdc += er32(MGTPDC);

	if (hw->mac.type >= e1000_pch_lpt) {
		u32 pbeccsts = er32(PBECCSTS);

		adapter->corr_errors +=
		    pbeccsts & E1000_PBECCSTS_CORR_ERR_CNT_MASK;
		adapter->uncorr_errors +=
		    FIELD_GET(E1000_PBECCSTS_UNCORR_ERR_CNT_MASK, pbeccsts);
	}
}

bool e1000e_has_link(struct e1000_adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	bool link_active = false;

	switch (hw->phy.media_type) {
	case e1000_media_type_copper:
		if (hw->mac.get_link_status) {
			hw->mac.ops.check_for_link(hw);
			link_active = !hw->mac.get_link_status;
		} else {
			link_active = true;
		}
		break;
	default:
		break;
	}

	return link_active;
}

static int e1000_init_phy_wakeup(struct e1000_adapter *adapter, u32 wufc)
{
	struct e1000_hw *hw = &adapter->hw;
	u32 i, mac_reg, wuc;
	u16 phy_reg, wuc_enable;
	int retval;

	e1000_copy_rx_addrs_to_phy_ich8lan(hw);

	retval = hw->phy.ops.acquire(hw);
	if (retval) {
		e_err("Could not acquire PHY\n");
		return retval;
	}

	retval = e1000_enable_phy_wakeup_reg_access_bm(hw, &wuc_enable);
	if (retval)
		goto release;

	for (i = 0; i < adapter->hw.mac.mta_reg_count; i++) {
		mac_reg = E1000_READ_REG_ARRAY(hw, E1000_MTA, i);
		hw->phy.ops.write_reg_page(hw, BM_MTA(i),
					   (u16)(mac_reg & 0xFFFF));
		hw->phy.ops.write_reg_page(hw, BM_MTA(i) + 1,
					   (u16)((mac_reg >> 16) & 0xFFFF));
	}

	hw->phy.ops.read_reg_page(&adapter->hw, BM_RCTL, &phy_reg);
	mac_reg = er32(RCTL);
	if (mac_reg & E1000_RCTL_UPE)
		phy_reg |= BM_RCTL_UPE;
	if (mac_reg & E1000_RCTL_MPE)
		phy_reg |= BM_RCTL_MPE;
	phy_reg &= ~(BM_RCTL_MO_MASK);
	if (mac_reg & E1000_RCTL_MO_3)
		phy_reg |= (FIELD_GET(E1000_RCTL_MO_3, mac_reg)
			    << BM_RCTL_MO_SHIFT);
	if (mac_reg & E1000_RCTL_BAM)
		phy_reg |= BM_RCTL_BAM;
	if (mac_reg & E1000_RCTL_PMCF)
		phy_reg |= BM_RCTL_PMCF;
	mac_reg = er32(CTRL);
	if (mac_reg & E1000_CTRL_RFCE)
		phy_reg |= BM_RCTL_RFCE;
	hw->phy.ops.write_reg_page(&adapter->hw, BM_RCTL, phy_reg);

	wuc = E1000_WUC_PME_EN;
	if (wufc & (E1000_WUFC_MAG | E1000_WUFC_LNKC))
		wuc |= E1000_WUC_APME;

	ew32(WUFC, wufc);
	ew32(WUC, (E1000_WUC_PHY_WAKE | E1000_WUC_APMPME |
		   E1000_WUC_PME_STATUS | wuc));

	hw->phy.ops.write_reg_page(&adapter->hw, BM_WUFC, (u16)wufc);
	hw->phy.ops.write_reg_page(&adapter->hw, BM_WUC, (u16)wuc);

	wuc_enable |= BM_WUC_ENABLE_BIT | BM_WUC_HOST_WU_BIT;
	retval = e1000_disable_phy_wakeup_reg_access_bm(hw, &wuc_enable);
	if (retval)
		e_err("Could not set PHY Host Wakeup bit\n");
release:
	hw->phy.ops.release(hw);

	return retval;
}

void e1000_suspend(struct e1000_adapter *adapter, u32 wufc)
{
	struct e1000_hw *hw = &adapter->hw;
	u32 ctrl, rctl, status;
	int retval = 0;

	e1000e_disable_pcie_master(&adapter->hw);

	status = er32(STATUS);
	if (status & E1000_STATUS_LU)
		wufc &= ~E1000_WUFC_LNKC;

	if (wufc) {
		e1000_setup_rctl(adapter);
		NdisAcquireSpinLock(&adapter->filter_lock);
		e1000e_set_rx_mode(adapter);
		NdisReleaseSpinLock(&adapter->filter_lock);

		if (wufc & E1000_WUFC_MC) {
			rctl = er32(RCTL);
			rctl |= E1000_RCTL_MPE;
			ew32(RCTL, rctl);
		}

		ctrl = er32(CTRL);
		ctrl |= E1000_CTRL_ADVD3WUC;
		if (!(adapter->flags2 & FLAG2_HAS_PHY_WAKEUP))
			ctrl |= E1000_CTRL_EN_PHY_PWR_MGMT;
		ew32(CTRL, ctrl);

		e1000e_power_up_phy(adapter);

		if (adapter->flags & FLAG_IS_ICH)
			e1000_suspend_workarounds_ich8lan(&adapter->hw);

		if (adapter->flags2 & FLAG2_HAS_PHY_WAKEUP) {
			retval = e1000_init_phy_wakeup(adapter, wufc);
			if (retval) {
				e_err("Failed to enable wakeup\n");
				goto skip_phy_configurations;
			}
		} else {
			ew32(WUFC, wufc);
			ew32(WUC, E1000_WUC_PME_EN);
		}
	} else {
		ew32(WUC, 0);
		ew32(WUFC, 0);

		e1000_power_down_phy(adapter);
	}

	if (hw->mac.type >= e1000_pch_lpt) {
		if (wufc && !(wufc & (E1000_WUFC_EX | E1000_WUFC_MC | E1000_WUFC_BC))) {
			retval = e1000_enable_ulp_lpt_lp(hw, true);
			if (retval) {
				e_err("Failed to enable ULP\n");
				goto skip_phy_configurations;
			}
		}
	}

	if ((hw->phy.type >= e1000_phy_i217) &&
	    adapter->eee_advert && hw->dev_spec.ich8lan.eee_lp_ability) {
		u16 lpi_ctrl = 0;

		retval = hw->phy.ops.acquire(hw);
		if (!retval) {
			retval = e1e_rphy_locked(hw, I82579_LPI_CTRL,
						 &lpi_ctrl);
			if (!retval) {
				if (adapter->eee_advert &
				    hw->dev_spec.ich8lan.eee_lp_ability &
				    I82579_EEE_100_SUPPORTED)
					lpi_ctrl |= I82579_LPI_CTRL_100_ENABLE;
				if (adapter->eee_advert &
				    hw->dev_spec.ich8lan.eee_lp_ability &
				    I82579_EEE_1000_SUPPORTED)
					lpi_ctrl |= I82579_LPI_CTRL_1000_ENABLE;

				retval = e1e_wphy_locked(hw, I82579_LPI_CTRL,
							 lpi_ctrl);
			}
		}
		hw->phy.ops.release(hw);
	}

skip_phy_configurations:
	e1000e_release_hw_control(adapter);
}

void e1000_resume(struct e1000_adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;

	if (hw->mac.type >= e1000_pch2lan)
		e1000_resume_workarounds_pchlan(&adapter->hw);

	e1000e_power_up_phy(adapter);

	if (adapter->flags2 & FLAG2_HAS_PHY_WAKEUP) {
		u16 phy_data;

		e1e_rphy(&adapter->hw, BM_WUS, &phy_data);
		if (phy_data) {
			e_info("PHY Wakeup cause - %s\n",
			       phy_data & E1000_WUS_EX ? "Unicast Packet" :
			       phy_data & E1000_WUS_MC ? "Multicast Packet" :
			       phy_data & E1000_WUS_BC ? "Broadcast Packet" :
			       phy_data & E1000_WUS_MAG ? "Magic Packet" :
			       phy_data & E1000_WUS_LNKC ?
			       "Link Status Change" : "other");
		}
		e1e_wphy(&adapter->hw, BM_WUS, ~0);
	} else {
		u32 wus = er32(WUS);

		if (wus) {
			e_info("MAC Wakeup cause - %s\n",
			       wus & E1000_WUS_EX ? "Unicast Packet" :
			       wus & E1000_WUS_MC ? "Multicast Packet" :
			       wus & E1000_WUS_BC ? "Broadcast Packet" :
			       wus & E1000_WUS_MAG ? "Magic Packet" :
			       wus & E1000_WUS_LNKC ? "Link Status Change" :
			       "other");
		}
		ew32(WUS, ~0);
	}

	e1000e_reset(adapter);

	e1000_init_manageability_pt(adapter);

	if (!(adapter->flags & FLAG_HAS_AMT))
		e1000e_get_hw_control(adapter);
}
