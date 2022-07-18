#include "QDMAController.h"
#include <map>
using namespace std;

map<char,Bars> device_list;
int num_device=0;
unsigned char default_pci_bus=0;

unsigned char get_pci_bus(unsigned char pci_bus){
	if(pci_bus==0){
		pci_bus=default_pci_bus;
	}
	if(device_list.count(pci_bus)==0){
		printf("Device %d has not been initialized\n",pci_bus);
		exit(1);
	}
	return pci_bus;
}
void init(unsigned char pci_bus, size_t bridge_bar_size){
	printf("Init pci dev: 0x%x\n",pci_bus);
	if(device_list.count(pci_bus) != 0){
		printf("device 0x%x has already been initialized!\n",pci_bus);
		exit(1);
	}else{
		if(device_list.size() == 0){
			default_pci_bus = pci_bus;
		}
		Bars t;
		device_list[pci_bus] = t;
	}
	char fname[256];
	int fd;
	unsigned char pci_dev 	=	0;
	unsigned char dev_func	=	0;

	//axi-lite
	get_syspath_bar_mmap(fname, pci_bus, pci_dev,dev_func, 2);//lite bar is 2
	fd = open(fname, O_RDWR);
	if (fd < 0){
		printf("Open lite error, maybe need sudo or you can check whether if %s exists\n",fname);
		exit(1);
	}
	device_list[pci_bus].lite_bar =(uint32_t*) mmap(NULL, 4*1024, PROT_WRITE, MAP_SHARED, fd, 0);
	if(device_list[pci_bus].lite_bar == MAP_FAILED){
		printf("MMAP lite bar error, please check fpga lite bar size in vivado\n");
		exit(1);
	}
	//axi-bridge
	get_syspath_bar_mmap(fname, pci_bus, pci_dev,dev_func, 4);//bridge bar is 4
	fd = open(fname, O_RDWR);
	if (fd < 0){
		printf("Open bridge error, maybe need sudo or you can check whether if %s exists\n",fname);
		exit(1);
	}
	device_list[pci_bus].bridge_bar =(__m512i*) mmap(NULL, bridge_bar_size, PROT_WRITE, MAP_SHARED|MAP_LOCKED , fd, 0);
	if(device_list[pci_bus].bridge_bar == MAP_FAILED){
		printf("MMAP bridge bar error, please check fpga bridge bar size in vivado\n");
		exit(1);
	}
	//config bar
	get_syspath_bar_mmap(fname, pci_bus, pci_dev,dev_func, 0);//config bar is 0
	fd = open(fname, O_RDWR);
	if (fd < 0){
		printf("Open config error, maybe need sudo or you can check whether if %s exists\n",fname);
		exit(1);
	}
	device_list[pci_bus].config_bar = (uint32_t *)mmap(NULL, 256*1024, PROT_WRITE, MAP_SHARED, fd, 0);
	if(device_list[pci_bus].config_bar == MAP_FAILED){
		printf("MMAP config bar error, please check fpga config bar size in vivado\n");
		exit(1);
	}
}

void* qdma_alloc(size_t size, unsigned char pci_bus, bool print_addr){
	pci_bus = get_pci_bus(pci_bus);
	int fd,hfd;
	void* huge_base;
	struct huge_mem hm;
	if ((fd = open("/dev/rc4ml_dev",O_RDWR)) == -1) {
		printf("[ERROR] on open /dev/rc4ml_dev, maybe you need to add 'sudo', or insmod\n");
		exit(1);
   	}
	char path[20];
	sprintf(path,"/media/huge/hfd_%x",pci_bus);
	if ((hfd = open(path, O_CREAT | O_RDWR | O_SYNC, 0755)) == -1) {
		printf("[ERROR] on open %s, maybe you need to add 'sudo'\n",path);
		exit(1);
   	}
	huge_base = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, hfd, 0);
	printf("huge pages base vaddr:%p\n", huge_base);
	hm.vaddr = (unsigned long)huge_base;
	hm.size = size;
	if(ioctl(fd, HUGE_MAPPING_SET, &hm) == -1){
		printf("IOCTL SET failed.\n");
	}
	struct huge_mapping map;
	map.nhpages = size/(2*1024*1024);
	map.phy_addr = (unsigned long*) calloc(map.nhpages, sizeof(unsigned long*));
	if (ioctl(fd, HUGE_MAPPING_GET, &map) == -1) {
    	printf("IOCTL GET failed.\n");
   	}

	tlb* page_table = (tlb*)calloc(1,sizeof(tlb));
	page_table->npages = map.nhpages;
	page_table->vaddr = (unsigned long*) calloc(map.nhpages, sizeof(unsigned long*));
	page_table->paddr = (unsigned long*) calloc(map.nhpages, sizeof(unsigned long*));
	for(int i=0;i<page_table->npages;i++){
		page_table->vaddr[i] = (unsigned long)huge_base + ((unsigned long)i)*2*1024*1024;
		page_table->paddr[i] = map.phy_addr[i];
	}

	for(int i=0;i<page_table->npages;i++){
		if(print_addr){
			printf("%lx %lx\n",page_table->vaddr[i],page_table->paddr[i]);
		}
		device_list[pci_bus].lite_bar[8]	= (uint32_t)(page_table->vaddr[i]);
		device_list[pci_bus].lite_bar[9]	= (uint32_t)((page_table->vaddr[i])>>32);
		device_list[pci_bus].lite_bar[10]	= (uint32_t)(page_table->paddr[i]);
		device_list[pci_bus].lite_bar[11]	= (uint32_t)((page_table->paddr[i])>>32);
		device_list[pci_bus].lite_bar[12]	= (i==0);
		device_list[pci_bus].lite_bar[13]	= 1;
		device_list[pci_bus].lite_bar[13]	= 0;
	}
	return huge_base;
}

void writeConfig(uint32_t index,uint32_t value, unsigned char pci_bus){
	pci_bus = get_pci_bus(pci_bus);
	device_list[pci_bus].config_bar[index] = value;
}
uint32_t readConfig(uint32_t index, unsigned char pci_bus){
	pci_bus = get_pci_bus(pci_bus);
	return device_list[pci_bus].config_bar[index];
}

void writeReg(uint32_t index,uint32_t value, unsigned char pci_bus){
	pci_bus = get_pci_bus(pci_bus);
	device_list[pci_bus].lite_bar[index] = value;
}
uint32_t readReg(uint32_t index, unsigned char pci_bus){
	pci_bus = get_pci_bus(pci_bus);
	return device_list[pci_bus].lite_bar[index];
}

void writeBridge(uint32_t index, uint64_t* value, unsigned char pci_bus){
	pci_bus = get_pci_bus(pci_bus);
	device_list[pci_bus].bridge_bar[index] = _mm512_set_epi64(value[7],value[6],value[5],value[4],value[3],value[2],value[1],value[0]);
}

void readBridge(uint32_t index, uint64_t* value, unsigned char pci_bus){
	pci_bus = get_pci_bus(pci_bus);
	_mm512_store_epi64(value,device_list[pci_bus].bridge_bar[index]);
}


void* getBridgeAddr(unsigned char pci_bus){
	pci_bus = get_pci_bus(pci_bus);
	return (void*)(device_list[pci_bus].bridge_bar);
}

void* getLiteAddr(unsigned char pci_bus){
	pci_bus = get_pci_bus(pci_bus);
	return (void*)(device_list[pci_bus].lite_bar);
}

void resetCounters(unsigned char pci_bus){
	pci_bus = get_pci_bus(pci_bus);
	device_list[pci_bus].lite_bar[14] = 1;
	device_list[pci_bus].lite_bar[14] = 0;
}

void printCounters(unsigned char pci_bus){
	pci_bus = get_pci_bus(pci_bus);
	volatile uint32_t *axi_lite = device_list[pci_bus].lite_bar;
	cout<<endl<<"QDMA debug info:"<<endl<<endl;
	printf("bar 1: 0x%x 	tlb miss count\n",	axi_lite[512+1]);

	cout<<endl<<"C2H CMD fire()"<<endl;
	printf("bar 2: 0x%x       io.c2h_cmd\n",	axi_lite[512+2]);
	printf("bar 3: 0x%x       check_c2h.io.out\n",	axi_lite[512+3]);
	printf("bar 5: 0x%x       tlb.io.c2h_out\n",	axi_lite[512+4]);
	printf("bar 4: 0x%x       boundary_split.io.cmd_out\n",	axi_lite[512+5]);
	printf("bar 6: 0x%x       fifo_c2h_cmd.io.out\n",	axi_lite[512+6]);

	cout<<endl<<"H2C CMD fire()"<<endl;
	printf("bar 7: 0x%x       io.h2c_cmd\n",	axi_lite[512+7]);
	printf("bar 8: 0x%x       check_h2c.io.out\n",	axi_lite[512+8]);
	printf("bar 9: 0x%x       tlb.io.h2c_out\n",	axi_lite[512+9]);
	printf("bar 10: 0x%x      fifo_h2c_cmd.io.out\n",	axi_lite[512+10]);

	cout<<endl<<"C2H DATA fire()"<<endl;
	printf("bar 11: 0x%x      io.c2h_data\n",	axi_lite[512+11]);
	printf("bar 12: 0x%x      boundary_split.io.data_out\n",	axi_lite[512+12]);
	printf("bar 13: 0x%x      fifo_c2h_data.io.out\n",	axi_lite[512+13]);

	cout<<endl<<"H2C DATA fire()"<<endl;
	printf("bar 14: 0x%x      io.h2c_data\n",	axi_lite[512+14]);
	printf("bar 15: 0x%x      fifo_h2c_data.io.in\n",	axi_lite[512+15]);

	//reporter
	cout<<endl;
	cout<<((axi_lite[512+16]>>0) & 1) << " Report 0:boundary check state===sIDLE"<<endl;
	cout<<((axi_lite[512+16]>>1) & 1) << " Report 1:boundary check state===sIDLE"<<endl;
	cout<<((axi_lite[512+16]>>2) & 1) << " Report 2:boundary split state===sIDLE"<<endl;
	cout<<endl;
	cout<<((axi_lite[512+16]>>3) & 1) << " Report 3:fifo_c2h_cmd.io.out.valid"<<endl;
	cout<<((axi_lite[512+16]>>4) & 1) << " Report 4:fifo_c2h_cmd.io.out.ready"<<endl;
	cout<<endl;
	cout<<((axi_lite[512+16]>>5) & 1) << " Report 5:fifo_h2c_cmd.io.out.valid"<<endl;
	cout<<((axi_lite[512+16]>>6) & 1) << " Report 6:fifo_h2c_cmd.io.out.ready"<<endl;
	cout<<endl;
	cout<<((axi_lite[512+16]>>7) & 1) << " Report 7:fifo_c2h_data.io.out.valid"<<endl;
	cout<<((axi_lite[512+16]>>8) & 1) << " Report 8:fifo_c2h_data.io.out.ready"<<endl;
	cout<<endl;
	cout<<((axi_lite[512+16]>>9) & 1)  << " Report 9:fifo_h2c_data.io.in.valid"<<endl;
	cout<<((axi_lite[512+16]>>10) & 1)  << " Report 10:fifo_h2c_data.io.in.ready"<<endl;
}