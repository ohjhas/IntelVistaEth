/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _E1000_OSDEP_H_
#define _E1000_OSDEP_H_

#include <ndis.h>

#pragma warning(disable: 4100)
#pragma warning(disable: 4127)
#pragma warning(disable: 4201)
#pragma warning(disable: 4214)
#pragma warning(disable: 4244)
#pragma warning(disable: 4245)
#pragma warning(disable: 4018)
#pragma warning(disable: 4389)
#pragma warning(disable: 4706)
#pragma warning(disable: 4701)
#pragma warning(disable: 4057)
#pragma warning(disable: 4146)

typedef UINT8	u8;
typedef UINT16	u16;
typedef UINT32	u32;
typedef UINT64	u64;
typedef INT8	s8;
typedef INT16	s16;
typedef INT32	s32;
typedef INT64	s64;

typedef u16 __le16;
typedef u32 __le32;
typedef u64 __le64;
typedef u64 dma_addr_t;

typedef int bool;
#define true	1
#define false	0

#define inline		__inline
#define __iomem
#define __always_unused
#define __maybe_unused
#define fallthrough	do { } while (0)
#define likely(x)	(x)
#define unlikely(x)	(x)

#define IS_ENABLED(x)	0

#define cpu_to_le16(x)	((u16)(x))
#define cpu_to_le32(x)	((u32)(x))
#define cpu_to_le64(x)	((u64)(x))
#define le16_to_cpu(x)	((u16)(x))
#define le32_to_cpu(x)	((u32)(x))
#define le64_to_cpu(x)	((u64)(x))

#define BIT(nr)			(1UL << (nr))
#define ARRAY_SIZE(a)		(sizeof(a) / sizeof((a)[0]))
#define DIV_ROUND_UP(n, d)	(((n) + (d) - 1) / (d))
#define ALIGN(x, a)		(((x) + ((a) - 1)) & ~((a) - 1))
#define DMA_BIT_MASK(n)		(((n) == 64) ? ~0ULL : ((1ULL << (n)) - 1))

#define min_t(type, a, b)	(((type)(a) < (type)(b)) ? (type)(a) : (type)(b))
#define max_t(type, a, b)	(((type)(a) > (type)(b)) ? (type)(a) : (type)(b))
#ifndef min
#define min(a, b)		(((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#define max(a, b)		(((a) > (b)) ? (a) : (b))
#endif

static __inline u32 e1000_mask_shift(u32 mask)
{
	unsigned long idx;

	if (!_BitScanForward(&idx, mask))
		return 0;
	return (u32)idx;
}

#define FIELD_GET(mask, val) \
	((u32)(((u32)(val) & (u32)(mask)) >> e1000_mask_shift((u32)(mask))))
#define FIELD_PREP(mask, val) \
	((u32)(((u32)(val) << e1000_mask_shift((u32)(mask))) & (u32)(mask)))

#define readl(a)	READ_REGISTER_ULONG((volatile ULONG *)(a))
#define writel(v, a)	WRITE_REGISTER_ULONG((volatile ULONG *)(a), (ULONG)(v))
#define readw(a)	READ_REGISTER_USHORT((volatile USHORT *)(a))
#define writew(v, a)	WRITE_REGISTER_USHORT((volatile USHORT *)(a), (USHORT)(v))

#define wmb()		KeMemoryBarrier()
#define rmb()		KeMemoryBarrier()
#define dma_rmb()	KeMemoryBarrier()
#define smp_mb()	KeMemoryBarrier()

void e1000_os_delay_us(u32 usecs);

#define udelay(us)		e1000_os_delay_us(us)
#define mdelay(ms)		e1000_os_delay_us((ms) * 1000)
#define msleep(ms)		e1000_os_delay_us((ms) * 1000)
#define usleep_range(lo, hi)	e1000_os_delay_us(lo)

#define test_and_set_bit(nr, addr) \
	((int)InterlockedBitTestAndSet((LONG volatile *)(addr), (LONG)(nr)))
#define set_bit(nr, addr) \
	((void)InterlockedBitTestAndSet((LONG volatile *)(addr), (LONG)(nr)))
#define clear_bit(nr, addr) \
	((void)InterlockedBitTestAndReset((LONG volatile *)(addr), (LONG)(nr)))
#define test_bit(nr, addr) \
	((int)((*(LONG volatile *)(addr) >> (nr)) & 1))

struct mutex {
	LONG volatile locked;
};

#define DEFINE_MUTEX(name)	struct mutex name = { 0 }

void mutex_lock(struct mutex *m);
void mutex_unlock(struct mutex *m);

struct pci_dev {
	NDIS_HANDLE	miniport;
	u16		vendor;
	u16		device;
	u16		subsystem_vendor;
	u16		subsystem_device;
	u8		revision;
	u8		pcie_cap;
};

int pci_read_config_word(struct pci_dev *pdev, int where, u16 *val);
int pci_write_config_word(struct pci_dev *pdev, int where, u16 val);

#define pci_pcie_cap(pdev)	((pdev)->pcie_cap)
int pcie_capability_read_word(struct pci_dev *pdev, int pos, u16 *val);

#define PCI_EXP_LNKSTA		0x12
#define PCI_EXP_LNKSTA_NLW	0x03f0

#define ETH_ALEN		6
#define ETH_HLEN		14
#define ETH_ZLEN		60
#define ETH_DATA_LEN		1500
#define ETH_FRAME_LEN		1514
#define ETH_FCS_LEN		4
#define VLAN_HLEN		4
#define VLAN_ETH_HLEN		18
#define VLAN_ETH_FRAME_LEN	1518
#define ETH_P_8021Q		0x8100

static __inline bool is_multicast_ether_addr(const u8 *addr)
{
	return (addr[0] & 0x01) != 0;
}

static __inline bool is_zero_ether_addr(const u8 *addr)
{
	return !(addr[0] | addr[1] | addr[2] | addr[3] | addr[4] | addr[5]);
}

static __inline bool is_valid_ether_addr(const u8 *addr)
{
	return !is_multicast_ether_addr(addr) && !is_zero_ether_addr(addr);
}

u32 ether_crc_le(int length, const u8 *data);

#define MII_BMCR		0x00
#define MII_BMSR		0x01
#define MII_PHYSID1		0x02
#define MII_PHYSID2		0x03
#define MII_ADVERTISE		0x04
#define MII_LPA			0x05
#define MII_EXPANSION		0x06
#define MII_CTRL1000		0x09
#define MII_STAT1000		0x0a
#define MII_ESTATUS		0x0f

#define BMCR_RESV		0x003f
#define BMCR_SPEED1000		0x0040
#define BMCR_CTST		0x0080
#define BMCR_FULLDPLX		0x0100
#define BMCR_ANRESTART		0x0200
#define BMCR_ISOLATE		0x0400
#define BMCR_PDOWN		0x0800
#define BMCR_ANENABLE		0x1000
#define BMCR_SPEED100		0x2000
#define BMCR_LOOPBACK		0x4000
#define BMCR_RESET		0x8000

#define BMSR_ERCAP		0x0001
#define BMSR_JCD		0x0002
#define BMSR_LSTATUS		0x0004
#define BMSR_ANEGCAPABLE	0x0008
#define BMSR_RFAULT		0x0010
#define BMSR_ANEGCOMPLETE	0x0020
#define BMSR_ESTATEN		0x0100
#define BMSR_10HALF		0x0800
#define BMSR_10FULL		0x1000
#define BMSR_100HALF		0x2000
#define BMSR_100FULL		0x4000

#define ADVERTISE_CSMA		0x0001
#define ADVERTISE_10HALF	0x0020
#define ADVERTISE_10FULL	0x0040
#define ADVERTISE_100HALF	0x0080
#define ADVERTISE_100FULL	0x0100
#define ADVERTISE_PAUSE_CAP	0x0400
#define ADVERTISE_PAUSE_ASYM	0x0800
#define ADVERTISE_ALL		(ADVERTISE_10HALF | ADVERTISE_10FULL | \
				 ADVERTISE_100HALF | ADVERTISE_100FULL)

#define LPA_10HALF		0x0020
#define LPA_10FULL		0x0040
#define LPA_100HALF		0x0080
#define LPA_100FULL		0x0100
#define LPA_PAUSE_CAP		0x0400
#define LPA_PAUSE_ASYM		0x0800

#define EXPANSION_NWAY		0x0001
#define EXPANSION_ENABLENPAGE	0x0004

#define ADVERTISE_1000HALF	0x0100
#define ADVERTISE_1000FULL	0x0200
#define CTL1000_AS_MASTER	0x0800
#define CTL1000_ENABLE_MASTER	0x1000

#define LPA_1000HALF		0x0400
#define LPA_1000FULL		0x0800
#define LPA_1000REMRXOK		0x1000
#define LPA_1000LOCALRXOK	0x2000

#define ADVERTISED_Pause	(1 << 13)
#define ADVERTISED_Asym_Pause	(1 << 14)

#define ESTATUS_1000_TFULL	0x2000
#define ESTATUS_1000_THALF	0x1000

#define MDIO_EEE_100TX		0x0002
#define MDIO_EEE_1000T		0x0004

#define SPEED_10		10
#define SPEED_100		100
#define SPEED_1000		1000

#define PCI_LTR_VALUE_MASK	0x000003ff
#define PCI_LTR_SCALE_MASK	0x00001c00
#define PCI_LTR_SCALE_SHIFT	10

#define ETH_TP_MDI_INVALID	0x00
#define ETH_TP_MDI		0x01
#define ETH_TP_MDI_X		0x02
#define ETH_TP_MDI_AUTO		0x03

#if DBG
#define e1000_print(fmt, ...) \
	DbgPrintEx(DPFLTR_IHVNETWORK_ID, DPFLTR_ERROR_LEVEL, "e1000ev: " fmt, __VA_ARGS__)
#else
#define e1000_print(fmt, ...)	do { } while (0)
#endif

#define e_dbg(fmt, ...)		e1000_print(fmt, __VA_ARGS__)
#define e_err(fmt, ...)		e1000_print(fmt, __VA_ARGS__)
#define e_info(fmt, ...)	e1000_print(fmt, __VA_ARGS__)
#define e_warn(fmt, ...)	e1000_print(fmt, __VA_ARGS__)
#define e_notice(fmt, ...)	e1000_print(fmt, __VA_ARGS__)

#endif
