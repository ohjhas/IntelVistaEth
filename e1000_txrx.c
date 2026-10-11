// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 1999 - 2018 Intel Corporation. */

#include "e1000.h"

#define ETH_P_IP	0x0800
#define ETH_P_IPV6	0x86DD
#define IPPROTO_TCP	6
#define IPPROTO_UDP	17

static u32 e1000_desc_unused(struct e1000_ring *ring)
{
	if (ring->next_to_clean > ring->next_to_use)
		return ring->next_to_clean - ring->next_to_use - 1;

	return ring->count + ring->next_to_clean - ring->next_to_use - 1;
}

static __inline u32 e1000_ring_next(struct e1000_ring *ring, u32 i)
{
	return (i + 1 == ring->count) ? 0 : i + 1;
}

static __inline u16 e1000_get_be16(const u8 *p)
{
	return (u16)((p[0] << 8) | p[1]);
}

static __inline void e1000_put_be16(u8 *p, u16 v)
{
	p[0] = (u8)(v >> 8);
	p[1] = (u8)v;
}

#define E1000_RX_PAIR_SIZE	(2 * E1000_RX_BUFFER_SIZE)

static NDIS_STATUS e1000_alloc_ring(struct e1000_adapter *adapter,
				    struct e1000_ring *ring, u32 count,
				    u32 desc_size)
{
	ring->count = count;
	ring->size = count * desc_size;
	ring->desc = NULL;

	NdisMAllocateSharedMemory(adapter->miniport, ring->size, FALSE,
				  &ring->desc, &ring->dma);
	if (!ring->desc)
		return NDIS_STATUS_RESOURCES;

	NdisZeroMemory(ring->desc, ring->size);
	ring->next_to_use = 0;
	ring->next_to_clean = 0;
	return NDIS_STATUS_SUCCESS;
}

static void *e1000_alloc_array(struct e1000_adapter *adapter, u32 bytes)
{
	void *p = NdisAllocateMemoryWithTagPriority(adapter->miniport, bytes,
						    E1000EV_POOL_TAG,
						    NormalPoolPriority);
	if (p)
		NdisZeroMemory(p, bytes);
	return p;
}

NDIS_STATUS e1000_alloc_rings(struct e1000_adapter *adapter)
{
	NET_BUFFER_LIST_POOL_PARAMETERS pool;
	NDIS_STATUS status;
	u32 tx_count = adapter->cfg.tx_ring;
	u32 rx_count = adapter->cfg.rx_ring;
	u32 i;

	adapter->tx_buf = e1000_alloc_array(adapter,
				tx_count * sizeof(struct e1000_tx_buffer));
	adapter->rx_slot = e1000_alloc_array(adapter,
				rx_count * sizeof(struct e1000_rx_buffer *));
	adapter->rx_buf_count = rx_count * 2;
	adapter->rx_bufs = e1000_alloc_array(adapter,
				adapter->rx_buf_count *
				sizeof(struct e1000_rx_buffer));
	if (!adapter->tx_buf || !adapter->rx_slot || !adapter->rx_bufs)
		return NDIS_STATUS_RESOURCES;

	status = e1000_alloc_ring(adapter, &adapter->tx_ring, tx_count,
				  sizeof(struct e1000_tx_desc));
	if (status != NDIS_STATUS_SUCCESS)
		return status;

	status = e1000_alloc_ring(adapter, &adapter->rx_ring, rx_count,
				  sizeof(union e1000_rx_desc_extended));
	if (status != NDIS_STATUS_SUCCESS)
		return status;

	for (i = 0; i < tx_count; i++) {
		NdisMAllocateSharedMemory(adapter->miniport,
					  E1000_TX_BUFFER_SIZE, TRUE,
					  &adapter->tx_buf[i].va,
					  &adapter->tx_buf[i].pa);
		if (!adapter->tx_buf[i].va)
			return NDIS_STATUS_RESOURCES;
	}

	NdisZeroMemory(&pool, sizeof(pool));
	pool.Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
	pool.Header.Revision = NET_BUFFER_LIST_POOL_PARAMETERS_REVISION_1;
	pool.Header.Size = NDIS_SIZEOF_NET_BUFFER_LIST_POOL_PARAMETERS_REVISION_1;
	pool.ProtocolId = NDIS_PROTOCOL_ID_DEFAULT;
	pool.fAllocateNetBuffer = TRUE;
	pool.PoolTag = E1000EV_POOL_TAG;
	adapter->nbl_pool = NdisAllocateNetBufferListPool(adapter->miniport,
							  &pool);
	if (!adapter->nbl_pool)
		return NDIS_STATUS_RESOURCES;

	adapter->rx_free.Next = NULL;
	adapter->rx_free_count = 0;
	for (i = 0; i < adapter->rx_buf_count; i += 2) {
		struct e1000_rx_buffer *b = &adapter->rx_bufs[i];
		void *va = NULL;
		NDIS_PHYSICAL_ADDRESS pa;

		NdisMAllocateSharedMemory(adapter->miniport,
					  E1000_RX_PAIR_SIZE, TRUE, &va, &pa);
		if (!va)
			return NDIS_STATUS_RESOURCES;

		b[0].va = va;
		b[0].pa = pa;
		b[1].va = (u8 *)va + E1000_RX_BUFFER_SIZE;
		pa.QuadPart += E1000_RX_BUFFER_SIZE;
		b[1].pa = pa;
	}

	for (i = 0; i < adapter->rx_buf_count; i++) {
		struct e1000_rx_buffer *buf = &adapter->rx_bufs[i];

		buf->adapter = adapter;
		buf->mdl = NdisAllocateMdl(adapter->miniport, buf->va,
					   E1000_RX_BUFFER_SIZE);
		if (!buf->mdl)
			return NDIS_STATUS_RESOURCES;

		buf->nbl = NdisAllocateNetBufferAndNetBufferList(
				adapter->nbl_pool, 0, 0, buf->mdl, 0,
				E1000_RX_BUFFER_SIZE);
		if (!buf->nbl)
			return NDIS_STATUS_RESOURCES;

		buf->nbl->SourceHandle = adapter->miniport;
		buf->nbl->MiniportReserved[0] = buf;
		PushEntryList(&adapter->rx_free, &buf->link);
		adapter->rx_free_count++;
	}

	return NDIS_STATUS_SUCCESS;
}

void e1000_free_rings(struct e1000_adapter *adapter)
{
	u32 i;

	if (adapter->rx_bufs) {
		for (i = 0; i < adapter->rx_buf_count; i++) {
			struct e1000_rx_buffer *buf = &adapter->rx_bufs[i];

			if (buf->nbl)
				NdisFreeNetBufferList(buf->nbl);
			if (buf->mdl)
				NdisFreeMdl(buf->mdl);
			buf->nbl = NULL;
			buf->mdl = NULL;
		}

		for (i = 0; i < adapter->rx_buf_count; i += 2) {
			if (adapter->rx_bufs[i].va)
				NdisMFreeSharedMemory(adapter->miniport,
						      E1000_RX_PAIR_SIZE, TRUE,
						      adapter->rx_bufs[i].va,
						      adapter->rx_bufs[i].pa);
		}

		NdisFreeMemory(adapter->rx_bufs, 0, 0);
		adapter->rx_bufs = NULL;
	}
	adapter->rx_free.Next = NULL;
	adapter->rx_free_count = 0;

	if (adapter->nbl_pool) {
		NdisFreeNetBufferListPool(adapter->nbl_pool);
		adapter->nbl_pool = NULL;
	}

	if (adapter->tx_buf) {
		for (i = 0; i < adapter->tx_ring.count; i++) {
			if (adapter->tx_buf[i].va)
				NdisMFreeSharedMemory(adapter->miniport,
						      E1000_TX_BUFFER_SIZE, TRUE,
						      adapter->tx_buf[i].va,
						      adapter->tx_buf[i].pa);
		}
		NdisFreeMemory(adapter->tx_buf, 0, 0);
		adapter->tx_buf = NULL;
	}

	if (adapter->rx_slot) {
		NdisFreeMemory(adapter->rx_slot, 0, 0);
		adapter->rx_slot = NULL;
	}

	if (adapter->rx_ring.desc) {
		NdisMFreeSharedMemory(adapter->miniport, adapter->rx_ring.size,
				      FALSE, adapter->rx_ring.desc,
				      adapter->rx_ring.dma);
		adapter->rx_ring.desc = NULL;
	}

	if (adapter->tx_ring.desc) {
		NdisMFreeSharedMemory(adapter->miniport, adapter->tx_ring.size,
				      FALSE, adapter->tx_ring.desc,
				      adapter->tx_ring.dma);
		adapter->tx_ring.desc = NULL;
	}
}

static void e1000_alloc_rx_buffers_locked(struct e1000_adapter *adapter)
{
	struct e1000_ring *rx_ring = &adapter->rx_ring;
	union e1000_rx_desc_extended *rx_desc;
	struct e1000_rx_buffer *buf;
	u32 cleaned_count = e1000_desc_unused(rx_ring);
	u32 i = rx_ring->next_to_use;

	if (!cleaned_count)
		return;

	while (cleaned_count--) {
		buf = adapter->rx_slot[i];
		if (!buf) {
			PSINGLE_LIST_ENTRY entry = PopEntryList(&adapter->rx_free);

			if (!entry)
				break;
			adapter->rx_free_count--;
			buf = CONTAINING_RECORD(entry, struct e1000_rx_buffer,
						link);
			adapter->rx_slot[i] = buf;
		}

		rx_desc = E1000_RX_DESC_EXT(*rx_ring, i);
		rx_desc->read.buffer_addr = cpu_to_le64(buf->pa.QuadPart);
		rx_desc->read.reserved = 0;

		i = e1000_ring_next(rx_ring, i);
	}

	if (i == rx_ring->next_to_use)
		return;

	rx_ring->next_to_use = i;

	wmb();
	if (adapter->hw_ready && !test_bit(__E1000_DOWN, &adapter->state))
		writel(i, rx_ring->tail);
}

void e1000_alloc_rx_buffers(struct e1000_adapter *adapter)
{
	struct e1000_ring *rx_ring = &adapter->rx_ring;

	NdisAcquireSpinLock(&adapter->rx_lock);
	e1000_alloc_rx_buffers_locked(adapter);
	writel(rx_ring->next_to_use, rx_ring->tail);
	NdisReleaseSpinLock(&adapter->rx_lock);
}

void e1000_clean_rx_ring(struct e1000_adapter *adapter)
{
	struct e1000_ring *rx_ring = &adapter->rx_ring;

	NdisAcquireSpinLock(&adapter->rx_lock);
	NdisZeroMemory(rx_ring->desc, rx_ring->size);
	rx_ring->next_to_clean = 0;
	rx_ring->next_to_use = 0;
	adapter->flags2 &= ~FLAG2_IS_DISCARDING;
	NdisReleaseSpinLock(&adapter->rx_lock);
}

static void e1000_count_frame(const u8 *dst, u32 len, u64 *ucast, u64 *mcast,
			      u64 *bcast, u64 *ucast_bytes, u64 *mcast_bytes,
			      u64 *bcast_bytes)
{
	if (is_multicast_ether_addr(dst)) {
		if (dst[0] == 0xff && dst[1] == 0xff && dst[2] == 0xff &&
		    dst[3] == 0xff && dst[4] == 0xff && dst[5] == 0xff) {
			(*bcast)++;
			*bcast_bytes += len;
		} else {
			(*mcast)++;
			*mcast_bytes += len;
		}
	} else {
		(*ucast)++;
		*ucast_bytes += len;
	}
}

static PVOID e1000_rx_checksum(struct e1000_adapter *adapter, const u8 *f,
			       u32 len, u32 staterr)
{
	struct e1000_offload_state *o = &adapter->off;
	NDIS_TCP_IP_CHECKSUM_NET_BUFFER_LIST_INFO ci;
	u8 status = (u8)staterr;
	u8 errors = (u8)(staterr >> 24);
	u32 l2 = ETH_HLEN;
	u16 type;
	u8 proto;
	bool l4_ok;

	ci.Value = NULL;

	if (status & E1000_RXD_STAT_IXSM)
		return ci.Value;
	if (len < ETH_HLEN + 4)
		return ci.Value;

	type = e1000_get_be16(f + 12);
	if (type == ETH_P_8021Q) {
		l2 += VLAN_HLEN;
		type = e1000_get_be16(f + 16);
	}

	l4_ok = !(errors & E1000_RXD_ERR_TCPE) &&
		(status & (E1000_RXD_STAT_TCPCS | E1000_RXD_STAT_UDPCS));

	if (type == ETH_P_IP && len >= l2 + 20) {
		if (o->rx_ip4 && (status & E1000_RXD_STAT_IPCS) &&
		    !(errors & E1000_RXD_ERR_IPE))
			ci.Receive.IpChecksumSucceeded = 1;

		if ((f[l2 + 6] & 0x3f) || f[l2 + 7])
			return ci.Value;

		proto = f[l2 + 9];
		if (l4_ok && proto == IPPROTO_TCP && o->rx_tcp4)
			ci.Receive.TcpChecksumSucceeded = 1;
		else if (l4_ok && proto == IPPROTO_UDP && o->rx_udp4)
			ci.Receive.UdpChecksumSucceeded = 1;
	} else if (type == ETH_P_IPV6 && len >= l2 + 40) {
		proto = f[l2 + 6];
		if (l4_ok && proto == IPPROTO_TCP && o->rx_tcp6)
			ci.Receive.TcpChecksumSucceeded = 1;
		else if (l4_ok && proto == IPPROTO_UDP && o->rx_udp6)
			ci.Receive.UdpChecksumSucceeded = 1;
	}

	return ci.Value;
}

static void e1000_rx_unchain(struct e1000_rx_buffer *buf, bool to_free_list)
{
	struct e1000_adapter *adapter = buf->adapter;
	struct e1000_rx_buffer *next;

	while (buf) {
		next = buf->chain;
		NDIS_MDL_LINKAGE(buf->mdl) = NULL;
		buf->chain = NULL;
		if (to_free_list) {
			PushEntryList(&adapter->rx_free, &buf->link);
			adapter->rx_free_count++;
		}
		buf = next;
	}
}

static void e1000_clean_rx_irq(struct e1000_adapter *adapter)
{
	struct e1000_ring *rx_ring = &adapter->rx_ring;
	PNET_BUFFER_LIST owned = NULL, *owned_tail = &owned;
	PNET_BUFFER_LIST copied = NULL, *copied_tail = &copied;
	ULONG owned_count = 0, copied_count = 0;
	u32 i, j, k, nd, length, staterr, processed = 0;
	u32 reserved = 0;

	NdisDprAcquireSpinLock(&adapter->rx_lock);

	i = rx_ring->next_to_clean;

	while (processed < rx_ring->count) {
		union e1000_rx_desc_extended *rx_desc;
		struct e1000_rx_buffer *first, *buf;
		NDIS_NET_BUFFER_LIST_8021Q_INFO q;
		PNET_BUFFER_LIST nbl;
		PNET_BUFFER nb;
		bool complete = false, drop = false;
		u16 vlan;

		j = i;
		nd = 0;
		length = 0;
		while (nd < rx_ring->count - processed) {
			rx_desc = E1000_RX_DESC_EXT(*rx_ring, j);
			staterr = le32_to_cpu(rx_desc->wb.upper.status_error);
			if (!(staterr & E1000_RXD_STAT_DD))
				break;
			nd++;
			length += le16_to_cpu(rx_desc->wb.upper.length);
			if (staterr & E1000_RXD_STAT_EOP) {
				complete = true;
				break;
			}
			j = e1000_ring_next(rx_ring, j);
		}
		if (!complete)
			break;

		dma_rmb();

		rx_desc = E1000_RX_DESC_EXT(*rx_ring, j);
		staterr = le32_to_cpu(rx_desc->wb.upper.status_error);
		vlan = le16_to_cpu(rx_desc->wb.upper.vlan);
		first = adapter->rx_slot[i];

		if (!(adapter->flags2 & FLAG2_CRC_STRIPPING))
			length -= 4;

		if (staterr & E1000_RXDEXT_ERR_FRAME_ERR_MASK) {
			adapter->rx_errors++;
			drop = true;
		} else if (length < ETH_HLEN ||
			   length > adapter->max_frame_size ||
			   nd * E1000_RX_BUFFER_SIZE < length) {
			adapter->rx_errors++;
			drop = true;
		} else if (adapter->ndis_state != E1000_NDIS_RUNNING ||
			   !adapter->packet_filter) {
			drop = true;
		}

		for (k = 0, buf = NULL; !drop && k < nd; k++)
			if (!adapter->rx_slot[(i + k) % rx_ring->count])
				drop = true;

		q.Value = NULL;
		if (!drop && (staterr & E1000_RXD_STAT_VP) &&
		    adapter->cfg.priority_vlan) {
			u16 vid = vlan & 0xFFF;

			if (adapter->vlan_id && vid && vid != adapter->vlan_id) {
				adapter->rx_vlan_dropped++;
				drop = true;
			} else {
				if (adapter->cfg.priority_vlan & 1)
					q.TagHeader.UserPriority = (vlan >> 13) & 7;
				if (adapter->cfg.priority_vlan & 2)
					q.TagHeader.VlanId = vid;
			}
		}

		if (!drop) {
			for (k = 0; k < nd; k++) {
				struct e1000_rx_buffer *b, *n;

				b = adapter->rx_slot[(i + k) % rx_ring->count];
				n = (k + 1 < nd) ?
				    adapter->rx_slot[(i + k + 1) % rx_ring->count] :
				    NULL;
				b->chain = n;
				NDIS_MDL_LINKAGE(b->mdl) = n ? n->mdl : NULL;
			}

			e1000_count_frame((const u8 *)first->va, length,
					  &adapter->rx_ucast, &adapter->rx_mcast,
					  &adapter->rx_bcast,
					  &adapter->rx_ucast_bytes,
					  &adapter->rx_mcast_bytes,
					  &adapter->rx_bcast_bytes);
			adapter->rx_ok++;
			adapter->rx_bytes += length;
			adapter->total_rx_packets++;
			adapter->total_rx_bytes += length;

			nbl = first->nbl;
			nb = NET_BUFFER_LIST_FIRST_NB(nbl);
			NET_BUFFER_DATA_LENGTH(nb) = length;
			NET_BUFFER_DATA_OFFSET(nb) = 0;
			NET_BUFFER_CURRENT_MDL(nb) = first->mdl;
			NET_BUFFER_CURRENT_MDL_OFFSET(nb) = 0;
			NET_BUFFER_LIST_NEXT_NBL(nbl) = NULL;
			NET_BUFFER_LIST_STATUS(nbl) = NDIS_STATUS_SUCCESS;
			NET_BUFFER_LIST_INFO(nbl, TcpIpChecksumNetBufferListInfo) =
				e1000_rx_checksum(adapter, (const u8 *)first->va,
						  min_t(u32, length,
							E1000_RX_BUFFER_SIZE),
						  staterr);
			NET_BUFFER_LIST_INFO(nbl, Ieee8021QNetBufferListInfo) =
				q.Value;

			if (adapter->rx_free_count >= reserved + nd) {
				for (k = 0; k < nd; k++)
					adapter->rx_slot[(i + k) % rx_ring->count] = NULL;
				reserved += nd;
				adapter->rx_outstanding++;
				*owned_tail = nbl;
				owned_tail = &NET_BUFFER_LIST_NEXT_NBL(nbl);
				owned_count++;
			} else {
				adapter->rx_nobuf++;
				*copied_tail = nbl;
				copied_tail = &NET_BUFFER_LIST_NEXT_NBL(nbl);
				copied_count++;
			}
		}

		for (k = 0; k < nd; k++) {
			rx_desc = E1000_RX_DESC_EXT(*rx_ring, i);
			rx_desc->wb.upper.status_error = 0;
			i = e1000_ring_next(rx_ring, i);
		}
		processed += nd;
	}
	rx_ring->next_to_clean = i;

	adapter->rx_indicating = true;
	NdisDprReleaseSpinLock(&adapter->rx_lock);

	if (owned)
		NdisMIndicateReceiveNetBufferLists(adapter->miniport, owned,
						   NDIS_DEFAULT_PORT_NUMBER,
						   owned_count,
						   NDIS_RECEIVE_FLAGS_DISPATCH_LEVEL);
	if (copied)
		NdisMIndicateReceiveNetBufferLists(adapter->miniport, copied,
						   NDIS_DEFAULT_PORT_NUMBER,
						   copied_count,
						   NDIS_RECEIVE_FLAGS_DISPATCH_LEVEL |
						   NDIS_RECEIVE_FLAGS_RESOURCES);

	NdisDprAcquireSpinLock(&adapter->rx_lock);
	while (copied) {
		PNET_BUFFER_LIST next = NET_BUFFER_LIST_NEXT_NBL(copied);

		NET_BUFFER_LIST_NEXT_NBL(copied) = NULL;
		e1000_rx_unchain(E1000_NBL_RXBUF(copied), false);
		copied = next;
	}
	adapter->rx_indicating = false;
	e1000_alloc_rx_buffers_locked(adapter);
	NdisDprReleaseSpinLock(&adapter->rx_lock);
}

VOID E1000ReturnNetBufferLists(NDIS_HANDLE MiniportAdapterContext,
			       PNET_BUFFER_LIST NetBufferLists,
			       ULONG ReturnFlags)
{
	struct e1000_adapter *adapter = MiniportAdapterContext;
	PNET_BUFFER_LIST nbl, next;
	bool pause_done = false;
	bool dispatch = NDIS_TEST_RETURN_AT_DISPATCH_LEVEL(ReturnFlags) != 0;

	e1000_lock(&adapter->rx_lock, dispatch);

	for (nbl = NetBufferLists; nbl; nbl = next) {
		next = NET_BUFFER_LIST_NEXT_NBL(nbl);
		NET_BUFFER_LIST_NEXT_NBL(nbl) = NULL;
		e1000_rx_unchain(E1000_NBL_RXBUF(nbl), true);
		adapter->rx_outstanding--;
	}

	if (!adapter->rx_indicating)
		e1000_alloc_rx_buffers_locked(adapter);

	if (adapter->ndis_state == E1000_NDIS_PAUSING &&
	    adapter->rx_outstanding == 0) {
		adapter->ndis_state = E1000_NDIS_PAUSED;
		pause_done = true;
	}

	e1000_unlock(&adapter->rx_lock, dispatch);

	if (pause_done)
		NdisMPauseComplete(adapter->miniport);
}

static u32 e1000_csum_add(const u8 *p, u32 len, u32 sum)
{
	while (len > 1) {
		sum += (p[0] << 8) | p[1];
		p += 2;
		len -= 2;
	}
	if (len)
		sum += p[0] << 8;
	return sum;
}

static u16 e1000_csum_fold(u32 sum)
{
	while (sum >> 16)
		sum = (sum & 0xffff) + (sum >> 16);
	return (u16)sum;
}

static u32 e1000_pseudo_sum(const u8 *l3, bool ipv6, u8 proto, u32 l4_len)
{
	u32 sum;

	if (ipv6)
		sum = e1000_csum_add(l3 + 8, 32, 0);
	else
		sum = e1000_csum_add(l3 + 12, 8, 0);
	sum += proto;
	sum += l4_len >> 16;
	sum += l4_len & 0xffff;
	return sum;
}

struct e1000_tx_hdr {
	u32	l3;
	u32	l4;
	u32	l4_hlen;
	bool	ipv6;
	bool	l4_ok;
	u8	proto;
};

static bool e1000_tx_parse(const u8 *f, u32 avail, u32 tcp_hint,
			   struct e1000_tx_hdr *h)
{
	u32 off = ETH_HLEN;
	u16 type;
	u8 nh;

	h->l4_ok = false;
	if (avail < ETH_HLEN + 20)
		return false;

	type = e1000_get_be16(f + 12);
	if (type == ETH_P_8021Q) {
		off += VLAN_HLEN;
		type = e1000_get_be16(f + 16);
	}
	h->l3 = off;

	if (type == ETH_P_IP) {
		u32 ihl = (f[off] & 0xf) * 4;

		if (avail < off + 20 || (f[off] >> 4) != 4 || ihl < 20 ||
		    avail < off + ihl)
			return false;
		h->ipv6 = false;
		h->proto = f[off + 9];
		h->l4 = off + ihl;
	} else if (type == ETH_P_IPV6) {
		if (avail < off + 40)
			return false;
		h->ipv6 = true;
		nh = f[off + 6];
		off += 40;
		while (nh == 0 || nh == 43 || nh == 60) {
			if (avail < off + 8)
				return true;
			nh = f[off];
			off += (f[off + 1] + 1) * 8;
		}
		h->proto = nh;
		h->l4 = off;
	} else {
		return false;
	}

	if (tcp_hint && h->proto == IPPROTO_TCP)
		h->l4 = tcp_hint;

	if (h->proto == IPPROTO_TCP) {
		if (avail < h->l4 + 20)
			return true;
		h->l4_hlen = (f[h->l4 + 12] >> 4) * 4;
		if (h->l4_hlen < 20)
			return true;
	} else if (h->proto == IPPROTO_UDP) {
		h->l4_hlen = 8;
	} else {
		return true;
	}

	h->l4_ok = avail >= h->l4 + h->l4_hlen;
	return true;
}

struct e1000_tx_req {
	u32	mss;
	u32	tcp_off;
	bool	lso_v2;
	bool	csum_ip;
	bool	csum_tcp;
	bool	csum_udp;
	u16	vlan_special;
	u32	lso_payload;
};

static bool e1000_tx_copy(struct e1000_adapter *adapter, PNET_BUFFER nb,
			  u32 slot, u32 len)
{
	struct e1000_ring *tx_ring = &adapter->tx_ring;
	PMDL mdl = NET_BUFFER_CURRENT_MDL(nb);
	u32 offset = NET_BUFFER_CURRENT_MDL_OFFSET(nb);
	u32 remaining = len;
	u32 dst_used = 0;
	u8 *dst = adapter->tx_buf[slot].va;

	while (mdl && remaining) {
		PVOID src = NULL;
		ULONG mdl_len = 0;

		NdisQueryMdl(mdl, &src, &mdl_len, NormalPagePriority);
		if (!src)
			return false;

		if (offset >= mdl_len) {
			offset -= mdl_len;
			mdl = NDIS_MDL_LINKAGE(mdl);
			continue;
		}

		mdl_len -= offset;
		while (mdl_len && remaining) {
			u32 chunk;

			if (dst_used == E1000_TX_BUFFER_SIZE) {
				slot = e1000_ring_next(tx_ring, slot);
				dst = adapter->tx_buf[slot].va;
				dst_used = 0;
			}
			chunk = min_t(u32, mdl_len, remaining);
			chunk = min_t(u32, chunk, E1000_TX_BUFFER_SIZE - dst_used);
			NdisMoveMemory(dst + dst_used, (u8 *)src + offset, chunk);
			dst_used += chunk;
			offset += chunk;
			mdl_len -= chunk;
			remaining -= chunk;
		}
		offset = 0;
		mdl = NDIS_MDL_LINKAGE(mdl);
	}

	return remaining == 0;
}

static int e1000_tx_frame(struct e1000_adapter *adapter, PNET_BUFFER nb,
			  struct e1000_tx_req *req)
{
	struct e1000_ring *tx_ring = &adapter->tx_ring;
	struct e1000_tx_hdr h;
	u32 len = NET_BUFFER_DATA_LENGTH(nb);
	u32 txd_lower = E1000_TXD_CMD_IFCS, txd_upper = 0;
	u32 nbufs, first, data_slot, i, k, remaining, hdr_len;
	u32 segs = 1, bytecount = len;
	bool parsed, l4_csum = false;
	u8 *f;
	struct e1000_tx_desc *tx_desc = NULL;

	if (len < ETH_HLEN)
		return -1;

	if (req->mss) {
		if (len > E1000_LSO_MAX_SIZE + 256)
			return -1;
	} else if (len > adapter->max_frame_size - ETH_FCS_LEN) {
		return -1;
	}

	nbufs = DIV_ROUND_UP(len, E1000_TX_BUFFER_SIZE);
	if (nbufs + 3 > tx_ring->count - 1)
		return -1;
	if (e1000_desc_unused(tx_ring) < nbufs + 3)
		return 0;

	first = tx_ring->next_to_use;

	if (req->mss || req->csum_tcp || req->csum_udp)
		data_slot = e1000_ring_next(tx_ring, first);
	else
		data_slot = first;
	if (!e1000_tx_copy(adapter, nb, data_slot, len))
		return -1;
	f = adapter->tx_buf[data_slot].va;

	parsed = false;
	if (req->mss || req->csum_ip || req->csum_tcp || req->csum_udp)
		parsed = e1000_tx_parse(f, min_t(u32, len, E1000_TX_BUFFER_SIZE),
					req->tcp_off, &h);

	if (req->mss) {
		struct e1000_context_desc *cd;
		u32 cmd_length = 0;
		u16 ipcse = 0;

		if (!parsed || !h.l4_ok || h.proto != IPPROTO_TCP)
			return -1;

		hdr_len = h.l4 + h.l4_hlen;
		if (hdr_len > 255 || hdr_len >= len)
			return -1;

		if (!h.ipv6) {
			f[h.l3 + 2] = 0;
			f[h.l3 + 3] = 0;
			f[h.l3 + 10] = 0;
			f[h.l3 + 11] = 0;
			cmd_length = E1000_TXD_CMD_IP;
			ipcse = (u16)(h.l4 - 1);
		} else {
			f[h.l3 + 4] = 0;
			f[h.l3 + 5] = 0;
		}
		e1000_put_be16(f + h.l4 + 16,
			       e1000_csum_fold(e1000_pseudo_sum(f + h.l3, h.ipv6,
								IPPROTO_TCP,
								0)));

		cmd_length |= (E1000_TXD_CMD_DEXT | E1000_TXD_CMD_TSE |
			       E1000_TXD_CMD_TCP | (len - hdr_len));

		cd = E1000_CONTEXT_DESC(*tx_ring, first);
		cd->lower_setup.ip_fields.ipcss = (u8)h.l3;
		cd->lower_setup.ip_fields.ipcso = (u8)(h.l3 + 10);
		cd->lower_setup.ip_fields.ipcse = cpu_to_le16(ipcse);
		cd->upper_setup.tcp_fields.tucss = (u8)h.l4;
		cd->upper_setup.tcp_fields.tucso = (u8)(h.l4 + 16);
		cd->upper_setup.tcp_fields.tucse = 0;
		cd->tcp_seg_setup.fields.status = 0;
		cd->tcp_seg_setup.fields.mss = cpu_to_le16((u16)req->mss);
		cd->tcp_seg_setup.fields.hdr_len = (u8)hdr_len;
		cd->cmd_and_length = cpu_to_le32(cmd_length);

		txd_lower |= E1000_TXD_CMD_DEXT | E1000_TXD_DTYP_D |
			     E1000_TXD_CMD_TSE;
		txd_upper |= E1000_TXD_POPTS_TXSM << 8;
		if (!h.ipv6)
			txd_upper |= E1000_TXD_POPTS_IXSM << 8;

		segs = DIV_ROUND_UP(len - hdr_len, req->mss);
		bytecount = (segs - 1) * hdr_len + len;
		req->lso_payload += len - hdr_len;
		adapter->tx_lso++;
		goto queue;
	}

	l4_csum = parsed && h.l4_ok &&
		  ((h.proto == IPPROTO_TCP && req->csum_tcp) ||
		   (h.proto == IPPROTO_UDP && req->csum_udp));

	if (!l4_csum && data_slot != first) {
		data_slot = first;
		if (!e1000_tx_copy(adapter, nb, data_slot, len))
			return -1;
		f = adapter->tx_buf[data_slot].va;
	}

	if (parsed && req->csum_ip && !h.ipv6) {
		u32 ihl = (f[h.l3] & 0xf) * 4;

		f[h.l3 + 10] = 0;
		f[h.l3 + 11] = 0;
		e1000_put_be16(f + h.l3 + 10,
			       (u16)~e1000_csum_fold(e1000_csum_add(f + h.l3,
								     ihl, 0)));
	}

	if (l4_csum) {
		struct e1000_context_desc *cd;
		u32 csum_off = h.l4 + (h.proto == IPPROTO_TCP ? 16 : 6);

		e1000_put_be16(f + csum_off,
			       e1000_csum_fold(e1000_pseudo_sum(f + h.l3, h.ipv6,
								h.proto,
								len - h.l4)));

		cd = E1000_CONTEXT_DESC(*tx_ring, first);
		cd->lower_setup.ip_config = 0;
		cd->upper_setup.tcp_fields.tucss = (u8)h.l4;
		cd->upper_setup.tcp_fields.tucso = (u8)csum_off;
		cd->upper_setup.tcp_fields.tucse = 0;
		cd->tcp_seg_setup.data = 0;
		cd->cmd_and_length = cpu_to_le32(E1000_TXD_CMD_DEXT |
			(h.proto == IPPROTO_TCP ? E1000_TXD_CMD_TCP : 0));

		txd_lower |= E1000_TXD_CMD_DEXT | E1000_TXD_DTYP_D;
		txd_upper |= E1000_TXD_POPTS_TXSM << 8;
	}

queue:
	if (req->vlan_special) {
		txd_lower |= E1000_TXD_CMD_VLE;
		txd_upper |= (u32)req->vlan_special << 16;
	}

	i = data_slot;
	remaining = len;
	for (k = 0; k < nbufs; k++) {
		u32 size = min_t(u32, remaining, E1000_TX_BUFFER_SIZE);

		tx_desc = E1000_TX_DESC(*tx_ring, i);
		tx_desc->buffer_addr = cpu_to_le64(adapter->tx_buf[i].pa.QuadPart);
		tx_desc->lower.data = cpu_to_le32(txd_lower | size);
		tx_desc->upper.data = cpu_to_le32(txd_upper);
		remaining -= size;
		if (k + 1 < nbufs)
			i = e1000_ring_next(tx_ring, i);
	}
	tx_desc->lower.data |= cpu_to_le32(adapter->txd_cmd);

	adapter->tx_buf[first].next_to_watch = i;
	adapter->tx_buf[first].segs = segs;
	adapter->tx_buf[first].bytecount = bytecount;
	tx_ring->next_to_use = e1000_ring_next(tx_ring, i);

	e1000_count_frame(f, len, &adapter->tx_ucast, &adapter->tx_mcast,
			  &adapter->tx_bcast, &adapter->tx_ucast_bytes,
			  &adapter->tx_mcast_bytes, &adapter->tx_bcast_bytes);
	adapter->tx_ok += segs;
	adapter->tx_bytes += bytecount;
	return 1;
}

static void e1000_tx_decode(struct e1000_adapter *adapter, PNET_BUFFER_LIST nbl,
			    struct e1000_tx_req *req)
{
	NDIS_TCP_LARGE_SEND_OFFLOAD_NET_BUFFER_LIST_INFO lso;
	NDIS_TCP_IP_CHECKSUM_NET_BUFFER_LIST_INFO csum;
	NDIS_NET_BUFFER_LIST_8021Q_INFO q;
	u16 vid, pri;

	NdisZeroMemory(req, sizeof(*req));

	lso.Value = NET_BUFFER_LIST_INFO(nbl, TcpLargeSendNetBufferListInfo);
	csum.Value = NET_BUFFER_LIST_INFO(nbl, TcpIpChecksumNetBufferListInfo);
	q.Value = NET_BUFFER_LIST_INFO(nbl, Ieee8021QNetBufferListInfo);

	if (lso.LsoV1Transmit.MSS) {
		req->mss = lso.LsoV1Transmit.MSS;
		if (lso.Transmit.Type == NDIS_TCP_LARGE_SEND_OFFLOAD_V2_TYPE) {
			req->lso_v2 = true;
			req->tcp_off = lso.LsoV2Transmit.TcpHeaderOffset;
		} else {
			req->tcp_off = lso.LsoV1Transmit.TcpHeaderOffset;
		}
	} else {
		req->csum_ip = csum.Transmit.IsIPv4 &&
			       csum.Transmit.IpHeaderChecksum;
		req->csum_tcp = csum.Transmit.TcpChecksum;
		req->csum_udp = csum.Transmit.UdpChecksum;
		if (req->csum_tcp)
			req->tcp_off = csum.Transmit.TcpHeaderOffset;
	}

	vid = (u16)q.TagHeader.VlanId;
	pri = (u16)q.TagHeader.UserPriority;
	if (!(adapter->cfg.priority_vlan & 2))
		vid = 0;
	else if (!vid)
		vid = adapter->vlan_id;
	if (!(adapter->cfg.priority_vlan & 1))
		pri = 0;
	if (vid || pri)
		req->vlan_special = (u16)((pri << 13) | vid);
}

static void e1000_tx_complete_info(PNET_BUFFER_LIST nbl,
				   struct e1000_tx_req *req)
{
	NDIS_TCP_LARGE_SEND_OFFLOAD_NET_BUFFER_LIST_INFO lso;

	if (!req->mss)
		return;

	lso.Value = NULL;
	if (req->lso_v2) {
		lso.LsoV2TransmitComplete.Type =
			NDIS_TCP_LARGE_SEND_OFFLOAD_V2_TYPE;
	} else {
		lso.LsoV1TransmitComplete.Type =
			NDIS_TCP_LARGE_SEND_OFFLOAD_V1_TYPE;
		lso.LsoV1TransmitComplete.TcpPayload = req->lso_payload;
	}
	NET_BUFFER_LIST_INFO(nbl, TcpLargeSendNetBufferListInfo) = lso.Value;
}

static void e1000_tx_drain_locked(struct e1000_adapter *adapter,
				  PNET_BUFFER_LIST **complete_tail)
{
	struct e1000_ring *tx_ring = &adapter->tx_ring;
	PNET_BUFFER_LIST nbl;
	bool queued = false;

	while ((nbl = adapter->tx_wait_head) != NULL) {
		NDIS_STATUS status = NDIS_STATUS_SUCCESS;
		struct e1000_tx_req req;
		PNET_BUFFER nb;
		u32 need = 0;

		e1000_tx_decode(adapter, nbl, &req);

		if (req.mss && !adapter->lso_capable) {
			status = NDIS_STATUS_FAILURE;
		} else {
			for (nb = NET_BUFFER_LIST_FIRST_NB(nbl); nb;
			     nb = NET_BUFFER_NEXT_NB(nb))
				need += DIV_ROUND_UP(NET_BUFFER_DATA_LENGTH(nb),
						     E1000_TX_BUFFER_SIZE) + 3;

			if (need > tx_ring->count - 1) {
				status = NDIS_STATUS_INVALID_LENGTH;
			} else if (e1000_desc_unused(tx_ring) < need) {
				adapter->tx_ring_full++;
				break;
			} else {
				for (nb = NET_BUFFER_LIST_FIRST_NB(nbl); nb;
				     nb = NET_BUFFER_NEXT_NB(nb)) {
					if (e1000_tx_frame(adapter, nb, &req) > 0) {
						queued = true;
					} else {
						adapter->tx_dropped++;
						status = NDIS_STATUS_INVALID_PACKET;
					}
				}
			}
		}

		adapter->tx_wait_head = NET_BUFFER_LIST_NEXT_NBL(nbl);
		if (!adapter->tx_wait_head)
			adapter->tx_wait_tail = NULL;

		if (status == NDIS_STATUS_SUCCESS)
			e1000_tx_complete_info(nbl, &req);
		NET_BUFFER_LIST_NEXT_NBL(nbl) = NULL;
		NET_BUFFER_LIST_STATUS(nbl) = status;
		**complete_tail = nbl;
		*complete_tail = &NET_BUFFER_LIST_NEXT_NBL(nbl);
	}

	if (queued) {
		wmb();
		writel(tx_ring->next_to_use, tx_ring->tail);
	}
}

static void e1000_complete_sends(struct e1000_adapter *adapter,
				 PNET_BUFFER_LIST list, bool dispatch)
{
	if (list)
		NdisMSendNetBufferListsComplete(adapter->miniport, list,
			dispatch ? NDIS_SEND_COMPLETE_FLAGS_DISPATCH_LEVEL : 0);
}

VOID E1000SendNetBufferLists(NDIS_HANDLE MiniportAdapterContext,
			     PNET_BUFFER_LIST NetBufferLists,
			     NDIS_PORT_NUMBER PortNumber,
			     ULONG SendFlags)
{
	struct e1000_adapter *adapter = MiniportAdapterContext;
	PNET_BUFFER_LIST nbl, next;
	PNET_BUFFER_LIST complete = NULL, *complete_tail = &complete;
	NDIS_STATUS fail = NDIS_STATUS_SUCCESS;
	bool dispatch = NDIS_TEST_SEND_AT_DISPATCH_LEVEL(SendFlags) != 0;

	e1000_lock(&adapter->tx_lock, dispatch);

	if (adapter->ndis_state != E1000_NDIS_RUNNING)
		fail = NDIS_STATUS_PAUSED;
	else if (adapter->power_state != NdisDeviceStateD0)
		fail = NDIS_STATUS_LOW_POWER_STATE;
	else if (!adapter->hw_ready ||
		 test_bit(__E1000_DOWN, &adapter->state))
		fail = NDIS_STATUS_RESET_IN_PROGRESS;
	else if (!adapter->link_up)
		fail = NDIS_STATUS_MEDIA_DISCONNECTED;

	for (nbl = NetBufferLists; nbl; nbl = next) {
		next = NET_BUFFER_LIST_NEXT_NBL(nbl);
		NET_BUFFER_LIST_NEXT_NBL(nbl) = NULL;

		if (fail != NDIS_STATUS_SUCCESS) {
			NET_BUFFER_LIST_STATUS(nbl) = fail;
			*complete_tail = nbl;
			complete_tail = &NET_BUFFER_LIST_NEXT_NBL(nbl);
			continue;
		}

		if (adapter->tx_wait_tail)
			NET_BUFFER_LIST_NEXT_NBL(adapter->tx_wait_tail) = nbl;
		else
			adapter->tx_wait_head = nbl;
		adapter->tx_wait_tail = nbl;
	}

	if (fail == NDIS_STATUS_SUCCESS)
		e1000_tx_drain_locked(adapter, &complete_tail);

	e1000_unlock(&adapter->tx_lock, dispatch);

	e1000_complete_sends(adapter, complete, dispatch);
}

VOID E1000CancelSend(NDIS_HANDLE MiniportAdapterContext, PVOID CancelId)
{
	struct e1000_adapter *adapter = MiniportAdapterContext;
	PNET_BUFFER_LIST nbl, next, prev = NULL;
	PNET_BUFFER_LIST complete = NULL, *complete_tail = &complete;

	NdisAcquireSpinLock(&adapter->tx_lock);

	for (nbl = adapter->tx_wait_head; nbl; nbl = next) {
		next = NET_BUFFER_LIST_NEXT_NBL(nbl);

		if (NDIS_GET_NET_BUFFER_LIST_CANCEL_ID(nbl) != CancelId) {
			prev = nbl;
			continue;
		}

		if (prev)
			NET_BUFFER_LIST_NEXT_NBL(prev) = next;
		else
			adapter->tx_wait_head = next;
		if (adapter->tx_wait_tail == nbl)
			adapter->tx_wait_tail = prev;

		NET_BUFFER_LIST_NEXT_NBL(nbl) = NULL;
		NET_BUFFER_LIST_STATUS(nbl) = NDIS_STATUS_SEND_ABORTED;
		*complete_tail = nbl;
		complete_tail = &NET_BUFFER_LIST_NEXT_NBL(nbl);
	}

	NdisReleaseSpinLock(&adapter->tx_lock);

	e1000_complete_sends(adapter, complete, false);
}

void e1000_tx_complete_all_waiting(struct e1000_adapter *adapter,
				   NDIS_STATUS status, bool dispatch)
{
	PNET_BUFFER_LIST nbl, list;

	e1000_lock(&adapter->tx_lock, dispatch);
	list = adapter->tx_wait_head;
	adapter->tx_wait_head = NULL;
	adapter->tx_wait_tail = NULL;
	e1000_unlock(&adapter->tx_lock, dispatch);

	for (nbl = list; nbl; nbl = NET_BUFFER_LIST_NEXT_NBL(nbl))
		NET_BUFFER_LIST_STATUS(nbl) = status;

	e1000_complete_sends(adapter, list, dispatch);
}

static void e1000_clean_tx_irq(struct e1000_adapter *adapter)
{
	struct e1000_ring *tx_ring = &adapter->tx_ring;
	PNET_BUFFER_LIST complete = NULL, *complete_tail = &complete;
	u32 i, eop;

	NdisDprAcquireSpinLock(&adapter->tx_lock);

	i = tx_ring->next_to_clean;
	while (i != tx_ring->next_to_use) {
		struct e1000_tx_desc *eop_desc;
		bool cleaned = false;

		eop = adapter->tx_buf[i].next_to_watch;
		eop_desc = E1000_TX_DESC(*tx_ring, eop);
		if (!(eop_desc->upper.data & cpu_to_le32(E1000_TXD_STAT_DD)))
			break;

		dma_rmb();
		adapter->total_tx_packets += adapter->tx_buf[i].segs;
		adapter->total_tx_bytes += adapter->tx_buf[i].bytecount;

		while (!cleaned) {
			cleaned = (i == eop);
			E1000_TX_DESC(*tx_ring, i)->upper.data = 0;
			i = e1000_ring_next(tx_ring, i);
		}
	}
	tx_ring->next_to_clean = i;

	if (adapter->tx_wait_head)
		e1000_tx_drain_locked(adapter, &complete_tail);

	NdisDprReleaseSpinLock(&adapter->tx_lock);

	e1000_complete_sends(adapter, complete, true);
}

void e1000_clean_tx_ring(struct e1000_adapter *adapter)
{
	struct e1000_ring *tx_ring = &adapter->tx_ring;

	NdisAcquireSpinLock(&adapter->tx_lock);
	NdisZeroMemory(tx_ring->desc, tx_ring->size);
	tx_ring->next_to_use = 0;
	tx_ring->next_to_clean = 0;
	adapter->tx_hang_ticks = 0;
	NdisReleaseSpinLock(&adapter->tx_lock);
}

void e1000_handle_interrupt(struct e1000_adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	u32 icr;

	InterlockedExchange(&adapter->dpc_queued, 0);
	icr = (u32)InterlockedExchange(&adapter->icr_pending, 0);

	if (test_bit(__E1000_DOWN, &adapter->state) || !adapter->hw_ready)
		goto out;

	if (icr & E1000_ICR_LSC) {
		hw->mac.get_link_status = true;
		e1000_schedule_watchdog(adapter);
	}

	if ((icr & E1000_ICR_ECCER) && (hw->mac.type >= e1000_pch_lpt)) {
		u32 pbeccsts = er32(PBECCSTS);

		adapter->corr_errors +=
		    pbeccsts & E1000_PBECCSTS_CORR_ERR_CNT_MASK;
		adapter->uncorr_errors +=
		    FIELD_GET(E1000_PBECCSTS_UNCORR_ERR_CNT_MASK, pbeccsts);

		e1000_schedule_reset(adapter);
		goto out;
	}

	adapter->total_tx_bytes = 0;
	adapter->total_tx_packets = 0;
	adapter->total_rx_bytes = 0;
	adapter->total_rx_packets = 0;

	e1000_clean_tx_irq(adapter);
	e1000_clean_rx_irq(adapter);

	if (adapter->itr_setting & 3)
		e1000_set_itr(adapter);

	if (!test_bit(__E1000_DOWN, &adapter->state))
		e1000_irq_enable(adapter);

out:
	InterlockedDecrement(&adapter->dpc_refs);
}

static bool e1000_service_icr(struct e1000_adapter *adapter, u32 icr)
{
	if (adapter->msi_test_ok == false && (icr & E1000_ICR_RXSEQ))
		adapter->msi_test_ok = true;

	if (test_bit(__E1000_DOWN, &adapter->state))
		return false;

	InterlockedOr(&adapter->icr_pending, (LONG)icr);

	if (InterlockedCompareExchange(&adapter->dpc_queued, 1, 0) == 0)
		InterlockedIncrement(&adapter->dpc_refs);

	return true;
}

BOOLEAN E1000Isr(NDIS_HANDLE MiniportInterruptContext,
		 PBOOLEAN QueueDefaultInterruptDpc,
		 PULONG TargetProcessors)
{
	struct e1000_adapter *adapter = MiniportInterruptContext;
	struct e1000_hw *hw = &adapter->hw;
	u32 icr;

	*QueueDefaultInterruptDpc = FALSE;
	*TargetProcessors = 0;

	if (!adapter->hw.hw_addr || adapter->power_state != NdisDeviceStateD0)
		return FALSE;

	icr = er32(ICR);

	if (!icr || icr == 0xFFFFFFFF)
		return FALSE;

	if (!(icr & E1000_ICR_INT_ASSERTED))
		return FALSE;

	if (e1000_service_icr(adapter, icr))
		*QueueDefaultInterruptDpc = TRUE;
	return TRUE;
}

VOID E1000InterruptDpc(NDIS_HANDLE MiniportInterruptContext,
		       PVOID MiniportDpcContext,
		       PVOID ReceiveThrottleParameters,
		       PVOID NdisReserved2)
{
	e1000_handle_interrupt((struct e1000_adapter *)MiniportInterruptContext);
}

VOID E1000EnableInterrupt(NDIS_HANDLE MiniportInterruptContext)
{
	struct e1000_adapter *adapter = MiniportInterruptContext;

	if (adapter->hw_ready && !test_bit(__E1000_DOWN, &adapter->state))
		e1000_irq_enable(adapter);
}

VOID E1000DisableInterrupt(NDIS_HANDLE MiniportInterruptContext)
{
	struct e1000_adapter *adapter = MiniportInterruptContext;
	struct e1000_hw *hw = &adapter->hw;

	if (adapter->hw.hw_addr && adapter->power_state == NdisDeviceStateD0)
		ew32(IMC, ~0);
}

BOOLEAN E1000MsiIsr(NDIS_HANDLE MiniportInterruptContext, ULONG MessageId,
		    PBOOLEAN QueueDefaultInterruptDpc, PULONG TargetProcessors)
{
	struct e1000_adapter *adapter = MiniportInterruptContext;
	struct e1000_hw *hw = &adapter->hw;
	u32 icr;

	*QueueDefaultInterruptDpc = FALSE;
	*TargetProcessors = 0;

	if (!adapter->hw.hw_addr || adapter->power_state != NdisDeviceStateD0)
		return TRUE;

	icr = er32(ICR);
	if (!icr || icr == 0xFFFFFFFF)
		return TRUE;

	if (e1000_service_icr(adapter, icr))
		*QueueDefaultInterruptDpc = TRUE;
	return TRUE;
}

VOID E1000MsiDpc(NDIS_HANDLE MiniportInterruptContext, ULONG MessageId,
		 PVOID MiniportDpcContext, PULONG NdisReserved1,
		 PULONG NdisReserved2)
{
	e1000_handle_interrupt((struct e1000_adapter *)MiniportInterruptContext);
}

VOID E1000MsiEnable(NDIS_HANDLE MiniportInterruptContext, ULONG MessageId)
{
	E1000EnableInterrupt(MiniportInterruptContext);
}

VOID E1000MsiDisable(NDIS_HANDLE MiniportInterruptContext, ULONG MessageId)
{
	E1000DisableInterrupt(MiniportInterruptContext);
}
