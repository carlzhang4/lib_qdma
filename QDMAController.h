#ifndef _QDMACONTROLLER_H_
#define _QDMACONTROLLER_H_

#include <iostream>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <immintrin.h>
#include <rc4ml.h>

void test();
void init(unsigned char pci_bus);
void writeReg(uint32_t index,uint32_t value);
uint32_t readReg(uint32_t index);
void* qdma_alloc(size_t size);
void writeBridge(uint32_t index, uint64_t* value);
void readBridge(uint32_t index, uint64_t* value);
volatile __m512i* get_bridge_addr();
#define get_syspath_bar_mmap(s, bus,dev,func,bar) \
	snprintf(s, sizeof(s), \
		"/sys/bus/pci/devices/0000:%02x:%02x.%x/resource%u", \
		bus, dev, func, bar)

typedef struct{
	int npages;
	unsigned long* vaddr;
	unsigned long* paddr;
}tlb;

#endif