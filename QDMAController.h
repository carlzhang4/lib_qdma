#ifndef _QDMACONTROLLER_H_
#define _QDMACONTROLLER_H_

#include <iostream>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <immintrin.h>
#include <rc4ml.h>

struct Bars{
	volatile uint32_t *config_bar;
	volatile uint32_t *lite_bar;
	volatile __m512i *bridge_bar;
};

void init(unsigned char pci_bus=0x1a,size_t bridge_bar_size=1*1024*1024*1024);
void writeConfig(uint32_t index,uint32_t value,unsigned char pci_bus=0);
uint32_t readConfig(uint32_t index,unsigned char pci_bus=0);
void writeReg(uint32_t index,uint32_t value,unsigned char pci_bus=0);
uint32_t readReg(uint32_t index,unsigned char pci_bus=0);
void* qdma_alloc(size_t size, unsigned char pci_bus=0, bool print_addr=0);
void writeBridge(uint32_t index, uint64_t* value,unsigned char pci_bus=0);
void readBridge(uint32_t index, uint64_t* value,unsigned char pci_bus=0);
void* getBridgeAddr(unsigned char pci_bus=0);
void* getLiteAddr(unsigned char pci_bus=0);
#define get_syspath_bar_mmap(s, bus,dev,func,bar) \
	snprintf(s, sizeof(s), \
		"/sys/bus/pci/devices/0000:%02x:%02x.%x/resource%u", \
		bus, dev, func, bar)

typedef struct{
	int npages;
	unsigned long* vaddr;
	unsigned long* paddr;
}tlb;

void resetCounters(unsigned char pci_bus=0);
void printCounters(unsigned char pci_bus=0);

#endif