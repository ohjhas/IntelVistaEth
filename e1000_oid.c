// SPDX-License-Identifier: GPL-2.0

#include "e1000.h"

#define E1000_SUPPORTED_FILTERS	(NDIS_PACKET_TYPE_DIRECTED | \
				 NDIS_PACKET_TYPE_MULTICAST | \
				 NDIS_PACKET_TYPE_ALL_MULTICAST | \
				 NDIS_PACKET_TYPE_BROADCAST | \
				 NDIS_PACKET_TYPE_PROMISCUOUS)

#define E1000_VENDOR_DESC	"Intel I217/I218/I219 (e1000e Vista port)"

NDIS_OID e1000_supported_oids[] = {
	OID_GEN_SUPPORTED_LIST,
	OID_GEN_HARDWARE_STATUS,
	OID_GEN_MEDIA_SUPPORTED,
	OID_GEN_MEDIA_IN_USE,
	OID_GEN_PHYSICAL_MEDIUM,
	OID_GEN_MAXIMUM_LOOKAHEAD,
	OID_GEN_MAXIMUM_FRAME_SIZE,
	OID_GEN_LINK_SPEED,
	OID_GEN_TRANSMIT_BUFFER_SPACE,
	OID_GEN_RECEIVE_BUFFER_SPACE,
	OID_GEN_TRANSMIT_BLOCK_SIZE,
	OID_GEN_RECEIVE_BLOCK_SIZE,
	OID_GEN_VENDOR_ID,
	OID_GEN_VENDOR_DESCRIPTION,
	OID_GEN_VENDOR_DRIVER_VERSION,
	OID_GEN_CURRENT_PACKET_FILTER,
	OID_GEN_CURRENT_LOOKAHEAD,
	OID_GEN_DRIVER_VERSION,
	OID_GEN_MAXIMUM_TOTAL_SIZE,
	OID_GEN_MAC_OPTIONS,
	OID_GEN_MEDIA_CONNECT_STATUS,
	OID_GEN_MAXIMUM_SEND_PACKETS,
	OID_GEN_XMIT_OK,
	OID_GEN_RCV_OK,
	OID_GEN_XMIT_ERROR,
	OID_GEN_RCV_ERROR,
	OID_GEN_RCV_NO_BUFFER,
	OID_GEN_RCV_CRC_ERROR,
	OID_GEN_STATISTICS,
	OID_GEN_INTERRUPT_MODERATION,
	OID_GEN_LINK_PARAMETERS,
	OID_GEN_VLAN_ID,
	OID_802_3_PERMANENT_ADDRESS,
	OID_802_3_CURRENT_ADDRESS,
	OID_802_3_MULTICAST_LIST,
	OID_802_3_MAXIMUM_LIST_SIZE,
	OID_802_3_RCV_ERROR_ALIGNMENT,
	OID_802_3_XMIT_ONE_COLLISION,
	OID_802_3_XMIT_MORE_COLLISIONS,
	OID_TCP_OFFLOAD_PARAMETERS,
	OID_OFFLOAD_ENCAPSULATION,
	OID_PNP_CAPABILITIES,
	OID_PNP_SET_POWER,
	OID_PNP_QUERY_POWER,
	OID_PNP_ENABLE_WAKE_UP,
};

ULONG e1000_supported_oids_size = sizeof(e1000_supported_oids);

void e1000_indicate_link(struct e1000_adapter *adapter)
{
	NDIS_LINK_STATE link;
	NDIS_STATUS_INDICATION ind;

	NdisZeroMemory(&link, sizeof(link));
	link.Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
	link.Header.Revision = NDIS_LINK_STATE_REVISION_1;
	link.Header.Size = NDIS_SIZEOF_LINK_STATE_REVISION_1;

	if (adapter->link_up) {
		u32 ctrl = readl(adapter->hw.hw_addr + E1000_CTRL);

		link.MediaConnectState = MediaConnectStateConnected;
		link.MediaDuplexState = adapter->link_duplex == FULL_DUPLEX ?
					MediaDuplexStateFull :
					MediaDuplexStateHalf;
		link.XmitLinkSpeed = (ULONG64)adapter->link_speed * 1000000;
		link.RcvLinkSpeed = link.XmitLinkSpeed;

		if ((ctrl & E1000_CTRL_TFCE) && (ctrl & E1000_CTRL_RFCE))
			link.PauseFunctions = NdisPauseFunctionsSendAndReceive;
		else if (ctrl & E1000_CTRL_TFCE)
			link.PauseFunctions = NdisPauseFunctionsSendOnly;
		else if (ctrl & E1000_CTRL_RFCE)
			link.PauseFunctions = NdisPauseFunctionsReceiveOnly;
		else
			link.PauseFunctions = NdisPauseFunctionsUnsupported;
	} else {
		link.MediaConnectState = MediaConnectStateDisconnected;
		link.MediaDuplexState = MediaDuplexStateUnknown;
		link.XmitLinkSpeed = NDIS_LINK_SPEED_UNKNOWN;
		link.RcvLinkSpeed = NDIS_LINK_SPEED_UNKNOWN;
		link.PauseFunctions = NdisPauseFunctionsUnknown;
	}

	if (adapter->hw.mac.autoneg)
		link.AutoNegotiationFlags =
			NDIS_LINK_STATE_XMIT_LINK_SPEED_AUTO_NEGOTIATED |
			NDIS_LINK_STATE_RCV_LINK_SPEED_AUTO_NEGOTIATED |
			NDIS_LINK_STATE_DUPLEX_AUTO_NEGOTIATED |
			NDIS_LINK_STATE_PAUSE_FUNCTIONS_AUTO_NEGOTIATED;

	NdisZeroMemory(&ind, sizeof(ind));
	ind.Header.Type = NDIS_OBJECT_TYPE_STATUS_INDICATION;
	ind.Header.Revision = NDIS_STATUS_INDICATION_REVISION_1;
	ind.Header.Size = NDIS_SIZEOF_STATUS_INDICATION_REVISION_1;
	ind.SourceHandle = adapter->miniport;
	ind.StatusCode = NDIS_STATUS_LINK_STATE;
	ind.StatusBuffer = &link;
	ind.StatusBufferSize = sizeof(link);

	NdisMIndicateStatusEx(adapter->miniport, &ind);
	adapter->link_indicated = true;
}

#define E1000_SUP(x)	((x) ? NDIS_OFFLOAD_SUPPORTED : NDIS_OFFLOAD_NOT_SUPPORTED)

void e1000_fill_offload(struct e1000_adapter *adapter, NDIS_OFFLOAD *o,
			bool hardware)
{
	struct e1000_offload_state *s = &adapter->off;
	ULONG encap = NDIS_ENCAPSULATION_IEEE_802_3;
	bool lso = adapter->lso_capable;

	NdisZeroMemory(o, sizeof(*o));
	o->Header.Type = NDIS_OBJECT_TYPE_OFFLOAD;
	o->Header.Revision = NDIS_OFFLOAD_REVISION_1;
	o->Header.Size = NDIS_SIZEOF_NDIS_OFFLOAD_REVISION_1;

	o->Checksum.IPv4Transmit.Encapsulation = encap;
	o->Checksum.IPv4Transmit.IpOptionsSupported = NDIS_OFFLOAD_SUPPORTED;
	o->Checksum.IPv4Transmit.TcpOptionsSupported = NDIS_OFFLOAD_SUPPORTED;
	o->Checksum.IPv4Transmit.TcpChecksum = E1000_SUP(hardware || s->tx_tcp4);
	o->Checksum.IPv4Transmit.UdpChecksum = E1000_SUP(hardware || s->tx_udp4);
	o->Checksum.IPv4Transmit.IpChecksum = E1000_SUP(hardware || s->tx_ip4);

	o->Checksum.IPv4Receive.Encapsulation = encap;
	o->Checksum.IPv4Receive.IpOptionsSupported = NDIS_OFFLOAD_SUPPORTED;
	o->Checksum.IPv4Receive.TcpOptionsSupported = NDIS_OFFLOAD_SUPPORTED;
	o->Checksum.IPv4Receive.TcpChecksum = E1000_SUP(hardware || s->rx_tcp4);
	o->Checksum.IPv4Receive.UdpChecksum = E1000_SUP(hardware || s->rx_udp4);
	o->Checksum.IPv4Receive.IpChecksum = E1000_SUP(hardware || s->rx_ip4);

	o->Checksum.IPv6Transmit.Encapsulation = encap;
	o->Checksum.IPv6Transmit.IpExtensionHeadersSupported = NDIS_OFFLOAD_SUPPORTED;
	o->Checksum.IPv6Transmit.TcpOptionsSupported = NDIS_OFFLOAD_SUPPORTED;
	o->Checksum.IPv6Transmit.TcpChecksum = E1000_SUP(hardware || s->tx_tcp6);
	o->Checksum.IPv6Transmit.UdpChecksum = E1000_SUP(hardware || s->tx_udp6);

	o->Checksum.IPv6Receive.Encapsulation = encap;
	o->Checksum.IPv6Receive.IpExtensionHeadersSupported = NDIS_OFFLOAD_NOT_SUPPORTED;
	o->Checksum.IPv6Receive.TcpOptionsSupported = NDIS_OFFLOAD_SUPPORTED;
	o->Checksum.IPv6Receive.TcpChecksum = E1000_SUP(hardware || s->rx_tcp6);
	o->Checksum.IPv6Receive.UdpChecksum = E1000_SUP(hardware || s->rx_udp6);

	if (lso && (hardware || s->lsov1)) {
		o->LsoV1.IPv4.Encapsulation = encap;
		o->LsoV1.IPv4.MaxOffLoadSize = E1000_LSO_MAX_SIZE;
		o->LsoV1.IPv4.MinSegmentCount = E1000_LSO_MIN_SEGMENTS;
		o->LsoV1.IPv4.TcpOptions = NDIS_OFFLOAD_SUPPORTED;
		o->LsoV1.IPv4.IpOptions = NDIS_OFFLOAD_SUPPORTED;
	}

	if (lso && (hardware || s->lsov2_ip4)) {
		o->LsoV2.IPv4.Encapsulation = encap;
		o->LsoV2.IPv4.MaxOffLoadSize = E1000_LSO_MAX_SIZE;
		o->LsoV2.IPv4.MinSegmentCount = E1000_LSO_MIN_SEGMENTS;
	}

	if (lso && (hardware || s->lsov2_ip6)) {
		o->LsoV2.IPv6.Encapsulation = encap;
		o->LsoV2.IPv6.MaxOffLoadSize = E1000_LSO_MAX_SIZE;
		o->LsoV2.IPv6.MinSegmentCount = E1000_LSO_MIN_SEGMENTS;
		o->LsoV2.IPv6.IpExtensionHeadersSupported = NDIS_OFFLOAD_SUPPORTED;
		o->LsoV2.IPv6.TcpOptionsSupported = NDIS_OFFLOAD_SUPPORTED;
	}
}

static void e1000_indicate_offload(struct e1000_adapter *adapter)
{
	NDIS_OFFLOAD o;
	NDIS_STATUS_INDICATION ind;

	e1000_fill_offload(adapter, &o, false);

	NdisZeroMemory(&ind, sizeof(ind));
	ind.Header.Type = NDIS_OBJECT_TYPE_STATUS_INDICATION;
	ind.Header.Revision = NDIS_STATUS_INDICATION_REVISION_1;
	ind.Header.Size = NDIS_SIZEOF_STATUS_INDICATION_REVISION_1;
	ind.SourceHandle = adapter->miniport;
	ind.StatusCode = NDIS_STATUS_TASK_OFFLOAD_CURRENT_CONFIG;
	ind.StatusBuffer = &o;
	ind.StatusBufferSize = sizeof(o);

	NdisMIndicateStatusEx(adapter->miniport, &ind);
}

static void e1000_apply_csum_param(UCHAR param, u8 *tx, u8 *rx)
{
	switch (param) {
	case NDIS_OFFLOAD_PARAMETERS_TX_RX_DISABLED:
		*tx = 0;
		*rx = 0;
		break;
	case NDIS_OFFLOAD_PARAMETERS_TX_ENABLED_RX_DISABLED:
		*tx = 1;
		*rx = 0;
		break;
	case NDIS_OFFLOAD_PARAMETERS_RX_ENABLED_TX_DISABLED:
		*tx = 0;
		*rx = 1;
		break;
	case NDIS_OFFLOAD_PARAMETERS_TX_RX_ENABLED:
		*tx = 1;
		*rx = 1;
		break;
	default:
		break;
	}
}

static void e1000_apply_lso_param(UCHAR param, u8 *on, bool capable)
{
	if (param == NDIS_OFFLOAD_PARAMETERS_LSOV2_DISABLED)
		*on = 0;
	else if (param == NDIS_OFFLOAD_PARAMETERS_LSOV2_ENABLED && capable)
		*on = 1;
}

static NDIS_STATUS e1000_set_offload_params(struct e1000_adapter *adapter,
					    PVOID in, ULONG in_len)
{
	NDIS_OFFLOAD_PARAMETERS *p = in;
	struct e1000_offload_state *s = &adapter->off;
	bool rx_was = s->rx_ip4 || s->rx_tcp4 || s->rx_udp4 || s->rx_tcp6 ||
		      s->rx_udp6;
	bool rx_now;

	if (in_len < NDIS_SIZEOF_OFFLOAD_PARAMETERS_REVISION_1)
		return NDIS_STATUS_INVALID_LENGTH;

	e1000_apply_csum_param(p->IPv4Checksum, &s->tx_ip4, &s->rx_ip4);
	e1000_apply_csum_param(p->TCPIPv4Checksum, &s->tx_tcp4, &s->rx_tcp4);
	e1000_apply_csum_param(p->UDPIPv4Checksum, &s->tx_udp4, &s->rx_udp4);
	e1000_apply_csum_param(p->TCPIPv6Checksum, &s->tx_tcp6, &s->rx_tcp6);
	e1000_apply_csum_param(p->UDPIPv6Checksum, &s->tx_udp6, &s->rx_udp6);
	e1000_apply_lso_param(p->LsoV1, &s->lsov1, adapter->lso_capable);
	e1000_apply_lso_param(p->LsoV2IPv4, &s->lsov2_ip4, adapter->lso_capable);
	e1000_apply_lso_param(p->LsoV2IPv6, &s->lsov2_ip6, adapter->lso_capable);

	rx_now = s->rx_ip4 || s->rx_tcp4 || s->rx_udp4 || s->rx_tcp6 ||
		 s->rx_udp6;
	if (rx_was != rx_now && adapter->hw_ready &&
	    !test_bit(__E1000_DOWN, &adapter->state)) {
		struct e1000_hw *hw = &adapter->hw;
		u32 rxcsum = er32(RXCSUM);

		if (rx_now)
			rxcsum |= E1000_RXCSUM_TUOFL | E1000_RXCSUM_IPOFL;
		else
			rxcsum &= ~(E1000_RXCSUM_TUOFL | E1000_RXCSUM_IPOFL);
		ew32(RXCSUM, rxcsum);
	}

	e1000_indicate_offload(adapter);
	return NDIS_STATUS_SUCCESS;
}

void e1000_fill_pm_caps(struct e1000_adapter *adapter,
			NDIS_PNP_CAPABILITIES *caps)
{
	NdisZeroMemory(caps, sizeof(*caps));
	caps->WakeUpCapabilities.MinMagicPacketWakeUp =
		((adapter->flags & FLAG_HAS_WOL) && adapter->cfg.wake_magic) ?
		NdisDeviceStateD3 : NdisDeviceStateUnspecified;
	caps->WakeUpCapabilities.MinPatternWakeUp = NdisDeviceStateUnspecified;
	caps->WakeUpCapabilities.MinLinkChangeWakeUp = NdisDeviceStateUnspecified;
}

static void e1000_fill_statistics(struct e1000_adapter *adapter,
				  NDIS_STATISTICS_INFO *s)
{
	NdisZeroMemory(s, sizeof(*s));
	s->Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
	s->Header.Revision = NDIS_STATISTICS_INFO_REVISION_1;
	s->Header.Size = NDIS_SIZEOF_STATISTICS_INFO_REVISION_1;
	s->SupportedStatistics =
		NDIS_STATISTICS_FLAGS_VALID_DIRECTED_FRAMES_RCV |
		NDIS_STATISTICS_FLAGS_VALID_MULTICAST_FRAMES_RCV |
		NDIS_STATISTICS_FLAGS_VALID_BROADCAST_FRAMES_RCV |
		NDIS_STATISTICS_FLAGS_VALID_BYTES_RCV |
		NDIS_STATISTICS_FLAGS_VALID_RCV_DISCARDS |
		NDIS_STATISTICS_FLAGS_VALID_RCV_ERROR |
		NDIS_STATISTICS_FLAGS_VALID_DIRECTED_FRAMES_XMIT |
		NDIS_STATISTICS_FLAGS_VALID_MULTICAST_FRAMES_XMIT |
		NDIS_STATISTICS_FLAGS_VALID_BROADCAST_FRAMES_XMIT |
		NDIS_STATISTICS_FLAGS_VALID_BYTES_XMIT |
		NDIS_STATISTICS_FLAGS_VALID_XMIT_ERROR |
		NDIS_STATISTICS_FLAGS_VALID_XMIT_DISCARDS |
		NDIS_STATISTICS_FLAGS_VALID_DIRECTED_BYTES_RCV |
		NDIS_STATISTICS_FLAGS_VALID_MULTICAST_BYTES_RCV |
		NDIS_STATISTICS_FLAGS_VALID_BROADCAST_BYTES_RCV |
		NDIS_STATISTICS_FLAGS_VALID_DIRECTED_BYTES_XMIT |
		NDIS_STATISTICS_FLAGS_VALID_MULTICAST_BYTES_XMIT |
		NDIS_STATISTICS_FLAGS_VALID_BROADCAST_BYTES_XMIT;

	s->ifInDiscards = adapter->rx_errors + adapter->stats.mpc +
			  adapter->stats.rnbc + adapter->rx_vlan_dropped;
	s->ifInErrors = adapter->rx_errors + adapter->stats.crcerrs +
			adapter->stats.algnerrc + adapter->stats.ruc +
			adapter->stats.roc + adapter->stats.cexterr;
	s->ifHCInOctets = adapter->rx_bytes;
	s->ifHCInUcastPkts = adapter->rx_ucast;
	s->ifHCInMulticastPkts = adapter->rx_mcast;
	s->ifHCInBroadcastPkts = adapter->rx_bcast;
	s->ifHCOutOctets = adapter->tx_bytes;
	s->ifHCOutUcastPkts = adapter->tx_ucast;
	s->ifHCOutMulticastPkts = adapter->tx_mcast;
	s->ifHCOutBroadcastPkts = adapter->tx_bcast;
	s->ifOutErrors = adapter->stats.ecol + adapter->stats.latecol;
	s->ifOutDiscards = adapter->tx_dropped;
	s->ifHCInUcastOctets = adapter->rx_ucast_bytes;
	s->ifHCInMulticastOctets = adapter->rx_mcast_bytes;
	s->ifHCInBroadcastOctets = adapter->rx_bcast_bytes;
	s->ifHCOutUcastOctets = adapter->tx_ucast_bytes;
	s->ifHCOutMulticastOctets = adapter->tx_mcast_bytes;
	s->ifHCOutBroadcastOctets = adapter->tx_bcast_bytes;
}

static void e1000_apply_filters(struct e1000_adapter *adapter)
{
	NdisAcquireSpinLock(&adapter->filter_lock);
	if (adapter->hw_ready && !test_bit(__E1000_DOWN, &adapter->state))
		e1000e_set_rx_mode(adapter);
	NdisReleaseSpinLock(&adapter->filter_lock);
}

static NDIS_STATUS e1000_query(struct e1000_adapter *adapter,
			       PNDIS_OID_REQUEST req)
{
	NDIS_OID oid = req->DATA.QUERY_INFORMATION.Oid;
	PVOID out = req->DATA.QUERY_INFORMATION.InformationBuffer;
	ULONG out_len = req->DATA.QUERY_INFORMATION.InformationBufferLength;
	NDIS_STATISTICS_INFO stats;
	NDIS_INTERRUPT_MODERATION_PARAMETERS imod;
	NDIS_PNP_CAPABILITIES pm;
	NDIS_OFFLOAD_ENCAPSULATION enc;
	ULONG ul = 0;
	ULONG64 ull = 0;
	PVOID src = &ul;
	ULONG len = sizeof(ULONG);
	bool counter = false;

	switch (oid) {
	case OID_GEN_SUPPORTED_LIST:
		src = e1000_supported_oids;
		len = e1000_supported_oids_size;
		break;

	case OID_GEN_HARDWARE_STATUS:
		ul = NdisHardwareStatusReady;
		break;

	case OID_GEN_MEDIA_SUPPORTED:
	case OID_GEN_MEDIA_IN_USE:
		ul = NdisMedium802_3;
		break;

	case OID_GEN_PHYSICAL_MEDIUM:
		ul = NdisPhysicalMedium802_3;
		break;

	case OID_GEN_MAXIMUM_LOOKAHEAD:
	case OID_GEN_CURRENT_LOOKAHEAD:
	case OID_GEN_MAXIMUM_FRAME_SIZE:
		ul = adapter->mtu;
		break;

	case OID_GEN_MAXIMUM_TOTAL_SIZE:
	case OID_GEN_TRANSMIT_BLOCK_SIZE:
	case OID_GEN_RECEIVE_BLOCK_SIZE:
		ul = adapter->mtu + ETH_HLEN;
		break;

	case OID_GEN_LINK_SPEED:
		ul = adapter->link_up ? adapter->link_speed * 10000 : 0;
		break;

	case OID_GEN_TRANSMIT_BUFFER_SPACE:
		ul = adapter->tx_ring.count * E1000_TX_BUFFER_SIZE;
		break;

	case OID_GEN_RECEIVE_BUFFER_SPACE:
		ul = adapter->rx_buf_count * E1000_RX_BUFFER_SIZE;
		break;

	case OID_GEN_VENDOR_ID:
		ul = ((ULONG)adapter->perm_addr[0] << 16) |
		     ((ULONG)adapter->perm_addr[1] << 8) |
		     adapter->perm_addr[2];
		ul = (ul << 8);
		break;

	case OID_GEN_VENDOR_DESCRIPTION:
		src = E1000_VENDOR_DESC;
		len = sizeof(E1000_VENDOR_DESC);
		break;

	case OID_GEN_VENDOR_DRIVER_VERSION:
		ul = (E1000EV_DRIVER_VERSION_MAJOR << 16) |
		     E1000EV_DRIVER_VERSION_MINOR;
		break;

	case OID_GEN_DRIVER_VERSION:
		ul = (6 << 8) | 0;
		len = sizeof(USHORT);
		break;

	case OID_GEN_CURRENT_PACKET_FILTER:
		ul = adapter->packet_filter;
		break;

	case OID_GEN_MAC_OPTIONS:
		ul = NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA |
		     NDIS_MAC_OPTION_TRANSFERS_NOT_PEND |
		     NDIS_MAC_OPTION_NO_LOOPBACK;
		if (adapter->cfg.priority_vlan & 1)
			ul |= NDIS_MAC_OPTION_8021P_PRIORITY;
		if (adapter->cfg.priority_vlan & 2)
			ul |= NDIS_MAC_OPTION_8021Q_VLAN;
		break;

	case OID_GEN_MEDIA_CONNECT_STATUS:
		ul = adapter->link_up ? NdisMediaStateConnected :
					NdisMediaStateDisconnected;
		break;

	case OID_GEN_MAXIMUM_SEND_PACKETS:
		ul = adapter->tx_ring.count - 1;
		break;

	case OID_GEN_XMIT_OK:
		ull = adapter->tx_ok;
		counter = true;
		break;

	case OID_GEN_RCV_OK:
		ull = adapter->rx_ok;
		counter = true;
		break;

	case OID_GEN_XMIT_ERROR:
		ull = adapter->stats.ecol + adapter->stats.latecol;
		counter = true;
		break;

	case OID_GEN_RCV_ERROR:
		ull = adapter->rx_errors + adapter->stats.crcerrs +
		      adapter->stats.algnerrc + adapter->stats.ruc +
		      adapter->stats.roc + adapter->stats.cexterr;
		counter = true;
		break;

	case OID_GEN_RCV_NO_BUFFER:
		ull = adapter->stats.mpc + adapter->stats.rnbc +
		      adapter->rx_nobuf;
		counter = true;
		break;

	case OID_GEN_RCV_CRC_ERROR:
		ull = adapter->stats.crcerrs;
		counter = true;
		break;

	case OID_802_3_RCV_ERROR_ALIGNMENT:
		ull = adapter->stats.algnerrc;
		counter = true;
		break;

	case OID_802_3_XMIT_ONE_COLLISION:
		ull = adapter->stats.scc;
		counter = true;
		break;

	case OID_802_3_XMIT_MORE_COLLISIONS:
		ull = adapter->stats.mcc;
		counter = true;
		break;

	case OID_GEN_STATISTICS:
		e1000_fill_statistics(adapter, &stats);
		src = &stats;
		len = sizeof(stats);
		break;

	case OID_GEN_INTERRUPT_MODERATION:
		NdisZeroMemory(&imod, sizeof(imod));
		imod.Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
		imod.Header.Revision = NDIS_INTERRUPT_MODERATION_PARAMETERS_REVISION_1;
		imod.Header.Size = NDIS_SIZEOF_INTERRUPT_MODERATION_PARAMETERS_REVISION_1;
		imod.Flags = 0;
		imod.InterruptModeration = adapter->itr_setting ?
					   NdisInterruptModerationEnabled :
					   NdisInterruptModerationDisabled;
		src = &imod;
		len = sizeof(imod);
		break;

	case OID_GEN_VLAN_ID:
		ul = adapter->vlan_id;
		break;

	case OID_802_3_PERMANENT_ADDRESS:
		src = adapter->perm_addr;
		len = ETH_ALEN;
		break;

	case OID_802_3_CURRENT_ADDRESS:
		src = adapter->curr_addr;
		len = ETH_ALEN;
		break;

	case OID_802_3_MULTICAST_LIST:
		src = adapter->mc_list;
		len = adapter->mc_count * ETH_ALEN;
		break;

	case OID_802_3_MAXIMUM_LIST_SIZE:
		ul = E1000_MAX_MCAST_LIST;
		break;

	case OID_OFFLOAD_ENCAPSULATION:
		NdisZeroMemory(&enc, sizeof(enc));
		enc.Header.Type = NDIS_OBJECT_TYPE_OFFLOAD_ENCAPSULATION;
		enc.Header.Revision = NDIS_OFFLOAD_ENCAPSULATION_REVISION_1;
		enc.Header.Size = NDIS_SIZEOF_OFFLOAD_ENCAPSULATION_REVISION_1;
		enc.IPv4.Enabled = adapter->encap_ip4 ? NDIS_OFFLOAD_SET_ON :
							NDIS_OFFLOAD_SET_OFF;
		enc.IPv4.EncapsulationType = NDIS_ENCAPSULATION_IEEE_802_3;
		enc.IPv4.HeaderSize = ETH_HLEN;
		enc.IPv6.Enabled = adapter->encap_ip6 ? NDIS_OFFLOAD_SET_ON :
							NDIS_OFFLOAD_SET_OFF;
		enc.IPv6.EncapsulationType = NDIS_ENCAPSULATION_IEEE_802_3;
		enc.IPv6.HeaderSize = ETH_HLEN;
		src = &enc;
		len = NDIS_SIZEOF_OFFLOAD_ENCAPSULATION_REVISION_1;
		break;

	case OID_PNP_CAPABILITIES:
		e1000_fill_pm_caps(adapter, &pm);
		src = &pm;
		len = sizeof(pm);
		break;

	case OID_PNP_QUERY_POWER:
		len = 0;
		break;

	case OID_PNP_ENABLE_WAKE_UP:
		ul = adapter->wake_flags;
		break;

	default:
		return NDIS_STATUS_NOT_SUPPORTED;
	}

	if (counter) {
		if (out_len >= sizeof(ULONG64)) {
			src = &ull;
			len = sizeof(ULONG64);
		} else {
			ul = (ULONG)ull;
			src = &ul;
			len = sizeof(ULONG);
		}
	}

	if (out_len < len) {
		req->DATA.QUERY_INFORMATION.BytesNeeded =
			counter ? sizeof(ULONG64) : len;
		req->DATA.QUERY_INFORMATION.BytesWritten = 0;
		return NDIS_STATUS_BUFFER_TOO_SHORT;
	}

	if (len)
		NdisMoveMemory(out, src, len);
	req->DATA.QUERY_INFORMATION.BytesWritten = len;
	req->DATA.QUERY_INFORMATION.BytesNeeded = 0;
	return NDIS_STATUS_SUCCESS;
}

static NDIS_STATUS e1000_set(struct e1000_adapter *adapter,
			     PNDIS_OID_REQUEST req)
{
	NDIS_OID oid = req->DATA.SET_INFORMATION.Oid;
	PVOID in = req->DATA.SET_INFORMATION.InformationBuffer;
	ULONG in_len = req->DATA.SET_INFORMATION.InformationBufferLength;
	NDIS_STATUS status;
	ULONG value;

	req->DATA.SET_INFORMATION.BytesRead = 0;
	req->DATA.SET_INFORMATION.BytesNeeded = 0;

	switch (oid) {
	case OID_GEN_CURRENT_PACKET_FILTER:
		if (in_len < sizeof(ULONG)) {
			req->DATA.SET_INFORMATION.BytesNeeded = sizeof(ULONG);
			return NDIS_STATUS_INVALID_LENGTH;
		}
		NdisMoveMemory(&value, in, sizeof(ULONG));
		if (value & ~E1000_SUPPORTED_FILTERS)
			return NDIS_STATUS_NOT_SUPPORTED;

		adapter->packet_filter = value;
		e1000_apply_filters(adapter);
		req->DATA.SET_INFORMATION.BytesRead = sizeof(ULONG);
		return NDIS_STATUS_SUCCESS;

	case OID_GEN_CURRENT_LOOKAHEAD:
		if (in_len < sizeof(ULONG)) {
			req->DATA.SET_INFORMATION.BytesNeeded = sizeof(ULONG);
			return NDIS_STATUS_INVALID_LENGTH;
		}
		req->DATA.SET_INFORMATION.BytesRead = sizeof(ULONG);
		return NDIS_STATUS_SUCCESS;

	case OID_802_3_MULTICAST_LIST:
		if (in_len % ETH_ALEN)
			return NDIS_STATUS_INVALID_LENGTH;
		if (in_len > sizeof(adapter->mc_list))
			return NDIS_STATUS_MULTICAST_FULL;

		NdisAcquireSpinLock(&adapter->filter_lock);
		NdisMoveMemory(adapter->mc_list, in, in_len);
		adapter->mc_count = in_len / ETH_ALEN;
		NdisReleaseSpinLock(&adapter->filter_lock);

		e1000_apply_filters(adapter);
		req->DATA.SET_INFORMATION.BytesRead = in_len;
		return NDIS_STATUS_SUCCESS;

	case OID_GEN_INTERRUPT_MODERATION: {
		NDIS_INTERRUPT_MODERATION_PARAMETERS *p = in;

		if (in_len < NDIS_SIZEOF_INTERRUPT_MODERATION_PARAMETERS_REVISION_1)
			return NDIS_STATUS_INVALID_LENGTH;

		if (p->InterruptModeration == NdisInterruptModerationDisabled) {
			adapter->itr_setting = 0;
			adapter->itr = 0;
		} else if (p->InterruptModeration == NdisInterruptModerationEnabled &&
			   !adapter->itr_setting) {
			adapter->itr_setting = 3;
			adapter->itr = E1000_ITR_START;
		}
		if (adapter->hw_ready && !test_bit(__E1000_DOWN, &adapter->state))
			e1000e_write_itr(adapter, adapter->itr);
		req->DATA.SET_INFORMATION.BytesRead = in_len;
		return NDIS_STATUS_SUCCESS;
	}

	case OID_GEN_VLAN_ID:
		if (in_len < sizeof(ULONG)) {
			req->DATA.SET_INFORMATION.BytesNeeded = sizeof(ULONG);
			return NDIS_STATUS_INVALID_LENGTH;
		}
		NdisMoveMemory(&value, in, sizeof(ULONG));
		if (value > 4094)
			return NDIS_STATUS_INVALID_DATA;
		if (value && !(adapter->cfg.priority_vlan & 2))
			return NDIS_STATUS_NOT_SUPPORTED;
		adapter->vlan_id = (u16)value;
		req->DATA.SET_INFORMATION.BytesRead = sizeof(ULONG);
		return NDIS_STATUS_SUCCESS;

	case OID_TCP_OFFLOAD_PARAMETERS:
		status = e1000_set_offload_params(adapter, in, in_len);
		if (status == NDIS_STATUS_SUCCESS)
			req->DATA.SET_INFORMATION.BytesRead = in_len;
		return status;

	case OID_OFFLOAD_ENCAPSULATION: {
		NDIS_OFFLOAD_ENCAPSULATION *e = in;

		if (in_len < NDIS_SIZEOF_OFFLOAD_ENCAPSULATION_REVISION_1)
			return NDIS_STATUS_INVALID_LENGTH;

		if ((e->IPv4.Enabled == NDIS_OFFLOAD_SET_ON &&
		     e->IPv4.EncapsulationType != NDIS_ENCAPSULATION_IEEE_802_3) ||
		    (e->IPv6.Enabled == NDIS_OFFLOAD_SET_ON &&
		     e->IPv6.EncapsulationType != NDIS_ENCAPSULATION_IEEE_802_3))
			return NDIS_STATUS_INVALID_PARAMETER;

		if (e->IPv4.Enabled == NDIS_OFFLOAD_SET_ON)
			adapter->encap_ip4 = 1;
		else if (e->IPv4.Enabled == NDIS_OFFLOAD_SET_OFF)
			adapter->encap_ip4 = 0;
		if (e->IPv6.Enabled == NDIS_OFFLOAD_SET_ON)
			adapter->encap_ip6 = 1;
		else if (e->IPv6.Enabled == NDIS_OFFLOAD_SET_OFF)
			adapter->encap_ip6 = 0;

		req->DATA.SET_INFORMATION.BytesRead = in_len;
		return NDIS_STATUS_SUCCESS;
	}

	case OID_PNP_SET_POWER:
		if (in_len < sizeof(NDIS_DEVICE_POWER_STATE)) {
			req->DATA.SET_INFORMATION.BytesNeeded =
				sizeof(NDIS_DEVICE_POWER_STATE);
			return NDIS_STATUS_INVALID_LENGTH;
		}
		req->DATA.SET_INFORMATION.BytesRead =
			sizeof(NDIS_DEVICE_POWER_STATE);
		return e1000_set_power(adapter,
				       *(NDIS_DEVICE_POWER_STATE *)in);

	case OID_PNP_ENABLE_WAKE_UP:
		if (in_len < sizeof(ULONG)) {
			req->DATA.SET_INFORMATION.BytesNeeded = sizeof(ULONG);
			return NDIS_STATUS_INVALID_LENGTH;
		}
		NdisMoveMemory(&value, in, sizeof(ULONG));
		adapter->wake_flags = value;
		req->DATA.SET_INFORMATION.BytesRead = sizeof(ULONG);
		return NDIS_STATUS_SUCCESS;

	case OID_GEN_LINK_PARAMETERS:
	case OID_PNP_ADD_WAKE_UP_PATTERN:
	case OID_PNP_REMOVE_WAKE_UP_PATTERN:
	default:
		return NDIS_STATUS_NOT_SUPPORTED;
	}
}

NDIS_STATUS E1000OidRequest(NDIS_HANDLE MiniportAdapterContext,
			    PNDIS_OID_REQUEST OidRequest)
{
	struct e1000_adapter *adapter = MiniportAdapterContext;

	switch (OidRequest->RequestType) {
	case NdisRequestQueryInformation:
	case NdisRequestQueryStatistics:
		return e1000_query(adapter, OidRequest);

	case NdisRequestSetInformation:
		return e1000_set(adapter, OidRequest);

	default:
		return NDIS_STATUS_NOT_SUPPORTED;
	}
}

VOID E1000CancelOidRequest(NDIS_HANDLE MiniportAdapterContext, PVOID RequestId)
{
}
