// SPDX-License-Identifier: GPL-2.0

#include "e1000.h"

void e1000_os_delay_us(u32 usecs)
{
	if (usecs > 2000 && KeGetCurrentIrql() <= APC_LEVEL) {
		LARGE_INTEGER interval;

		interval.QuadPart = -((LONGLONG)usecs * 10);
		KeDelayExecutionThread(KernelMode, FALSE, &interval);
		return;
	}

	while (usecs) {
		u32 chunk = usecs > 50 ? 50 : usecs;

		KeStallExecutionProcessor(chunk);
		usecs -= chunk;
	}
}

void mutex_lock(struct mutex *m)
{
	while (InterlockedCompareExchange(&m->locked, 1, 0) != 0)
		e1000_os_delay_us(1000);
}

void mutex_unlock(struct mutex *m)
{
	InterlockedExchange(&m->locked, 0);
}

int pci_read_config_word(struct pci_dev *pdev, int where, u16 *val)
{
	ULONG got;

	got = NdisMGetBusData(pdev->miniport, PCI_WHICHSPACE_CONFIG,
			      (ULONG)where, val, sizeof(*val));
	if (got != sizeof(*val)) {
		*val = 0;
		return -1;
	}
	return 0;
}

int pci_write_config_word(struct pci_dev *pdev, int where, u16 val)
{
	ULONG put;

	put = NdisMSetBusData(pdev->miniport, PCI_WHICHSPACE_CONFIG,
			      (ULONG)where, &val, sizeof(val));
	return put == sizeof(val) ? 0 : -1;
}

int pcie_capability_read_word(struct pci_dev *pdev, int pos, u16 *val)
{
	if (!pdev->pcie_cap) {
		*val = 0;
		return -1;
	}
	return pci_read_config_word(pdev, pdev->pcie_cap + pos, val);
}

u32 ether_crc_le(int length, const u8 *data)
{
	u32 crc = 0xFFFFFFFF;
	int bit;

	while (length-- > 0) {
		crc ^= *data++;
		for (bit = 0; bit < 8; bit++)
			crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
	}
	return crc;
}
