#include "QDMAController.h"

using namespace std;

bool init_done=false;
volatile uint32_t *config_bar;
volatile uint32_t *axi_lite;
volatile __m512i *axi_bridge;



void init(unsigned char pci_bus=0x1a){
	uint32_t barno;
	char fname[256];
	int fd;
	unsigned char pci_dev 	=	0;
	unsigned char dev_func	=	0;

	//axi-lite
	barno = 2;
	get_syspath_bar_mmap(fname, pci_bus, pci_dev,dev_func, barno);
	printf("%s\n",fname);
	fd = open(fname, (PROT_WRITE & PROT_WRITE) ? O_RDWR : O_RDONLY);
	if (fd < 0)
		printf("Open lite error, maybe need sudo\n");
	axi_lite =(uint32_t*) mmap(NULL, 4*1024, PROT_WRITE, MAP_SHARED, fd, 0);

	//axi-bridge
	barno = 4;
	get_syspath_bar_mmap(fname, pci_bus, pci_dev,dev_func, barno);
	printf("%s\n",fname);
	fd = open(fname, (PROT_WRITE & PROT_WRITE) ? O_RDWR : O_RDONLY);
	if (fd < 0)
		printf("Open bridge error, maybe need sudo\n");
	axi_bridge =(__m512i*) mmap(NULL, 1*1024*1024*1024, PROT_WRITE, MAP_SHARED, fd, 0);

	//config bar
	barno = 0;
	get_syspath_bar_mmap(fname, pci_bus, pci_dev,dev_func, barno);
	printf("%s\n",fname);
	fd = open(fname, (PROT_WRITE & PROT_WRITE) ? O_RDWR : O_RDONLY);
	if (fd < 0)
		printf("Error\n");
	config_bar = (uint32_t *)mmap(NULL, 256*1024, PROT_WRITE, MAP_SHARED, fd, 0);
	init_done=true;
}

void* qdma_alloc(size_t size, bool print_addr){
	if(!init_done)
		printf("Please Init First\n");
	int fd,hfd;
	void* huge_base;
	struct huge_mem hm;
	if ((fd = open("/dev/rc4ml_dev",O_RDWR)) == -1) {
		printf("[ERROR] on open /dev/rc4ml_dev, maybe you need to add 'sudo', or insmod\n");
		exit(1);
   	}
	if ((hfd = open("/media/huge/abc", O_CREAT | O_RDWR | O_SYNC, 0755)) == -1) {
		printf("[ERROR] on open /media/huge/abc, maybe you need to add 'sudo'\n");
		exit(1);
   	}
	huge_base = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, hfd, 0);
	printf("huge device mapped at vaddr:%p\n", huge_base);
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
		page_table->vaddr[i] = (unsigned long)huge_base + i*2*1024*1024;
		page_table->paddr[i] = map.phy_addr[i];
	}

	for(int i=0;i<page_table->npages;i++){
		if(print_addr){
			printf("%lx %lx\n",page_table->vaddr[i],page_table->paddr[i]);
		}
		axi_lite[8]	= (uint32_t)(page_table->vaddr[i]);
		axi_lite[9]	= (uint32_t)((page_table->vaddr[i])>>32);
		axi_lite[10] = (uint32_t)(page_table->paddr[i]);
		axi_lite[11] = (uint32_t)((page_table->paddr[i])>>32);
		axi_lite[12] = (i==0);
		axi_lite[13] = 1;
		axi_lite[13] = 0;
	}

	return huge_base;

}

void writeConfig(uint32_t index,uint32_t value){
	if(init_done)
		config_bar[index] = value;
	else{
		printf("Please Init First\n");
	}
}
uint32_t readConfig(uint32_t index){
	if(init_done)
		return config_bar[index];
	else{
		printf("Please Init First\n");
	}
}

void writeReg(uint32_t index,uint32_t value){
	if(init_done)
		axi_lite[index] = value;
	else{
		printf("Please Init First\n");
	}
}
uint32_t readReg(uint32_t index){
	if(init_done)
		return axi_lite[index];
	else{
		printf("Please Init First\n");
	}
}

void writeBridge(uint32_t index, uint64_t* value){
	if(init_done)
		axi_bridge[index] = _mm512_set_epi64(value[7],value[6],value[5],value[4],value[3],value[2],value[1],value[0]);
	else{
		printf("Please Init First\n");
	}
}

void readBridge(uint32_t index, uint64_t* value){
	if(init_done)
		_mm512_store_epi64(value,axi_bridge[index]);
	else{
		printf("Please Init First\n");
	}
}


void* getBridgeAddr(){
	return (void*)axi_bridge;
}

void* getLiteAddr(){
	return (void*)axi_lite;
}

void resetCounters(){
	axi_lite[14] = 1;
	axi_lite[14] = 0;
}

void printCounters(){
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
	cout<<((axi_lite[512+16]>>1) & 1) << " Report 1:boundary check state===sFIRSTCMD"<<endl;
	cout<<((axi_lite[512+16]>>2) & 1) << " Report 2:boundary check state===sSPLIT"<<endl;
	cout<<((axi_lite[512+16]>>3) & 1) << " Report 3:boundary check state===sMINISPLIT"<<endl;
	cout<<((axi_lite[512+16]>>4) & 1) << " Report 4:boundary check state===sLASTSPLIT"<<endl;
	cout<<((axi_lite[512+16]>>5) & 1) << " Report 5:boundary check state===sIDLE"<<endl;
	cout<<((axi_lite[512+16]>>6) & 1) << " Report 6:boundary check state===sFIRSTCMD"<<endl;
	cout<<((axi_lite[512+16]>>7) & 1) << " Report 7:boundary check state===sSPLIT"<<endl;
	cout<<((axi_lite[512+16]>>8) & 1) << " Report 8:boundary check state===sMINISPLIT"<<endl;
	cout<<((axi_lite[512+16]>>9) & 1) << " Report 9:boundary check state===sLASTSPLIT"<<endl;
	cout<<((axi_lite[512+16]>>10) & 1) << " Report 10:boundary split state===sIDLE"<<endl;
	cout<<((axi_lite[512+16]>>11) & 1) << " Report 11:boundary split state===sREAD_DATA"<<endl;
	cout<<endl;
	cout<<((axi_lite[512+16]>>12) & 1) << " Report 12:io.c2h_cmd.valid"<<endl;
	cout<<((axi_lite[512+16]>>13) & 1) << " Report 13:io.c2h_cmd.ready"<<endl;
	cout<<((axi_lite[512+16]>>14) & 1) << " Report 14:check_c2h.io.out.valid"<<endl;
	cout<<((axi_lite[512+16]>>15) & 1) << " Report 15:check_c2h.io.out.ready"<<endl;
	cout<<((axi_lite[512+16]>>16) & 1) << " Report 16:tlb.io.c2h_out.valid"<<endl;
	cout<<((axi_lite[512+16]>>17) & 1) << " Report 17:tlb.io.c2h_out.ready"<<endl;
	cout<<((axi_lite[512+16]>>18) & 1) << " Report 18:boundary_split.io.cmd_out.valid"<<endl;
	cout<<((axi_lite[512+16]>>19) & 1) << " Report 19:boundary_split.io.cmd_out.ready"<<endl;
	cout<<((axi_lite[512+16]>>20) & 1) << " Report 20:fifo_c2h_cmd.io.out.valid"<<endl;
	cout<<((axi_lite[512+16]>>21) & 1) << " Report 21:fifo_c2h_cmd.io.out.ready"<<endl;
	cout<<endl;
	cout<<((axi_lite[512+16]>>22) & 1) << " Report 22:io.h2c_cmd.valid"<<endl;
	cout<<((axi_lite[512+16]>>23) & 1) << " Report 23:io.h2c_cmd.ready"<<endl;
	cout<<((axi_lite[512+16]>>24) & 1) << " Report 24:check_h2c.io.out.valid"<<endl;
	cout<<((axi_lite[512+16]>>25) & 1) << " Report 25:check_h2c.io.out.ready"<<endl;
	cout<<((axi_lite[512+16]>>26) & 1) << " Report 26:tlb.io.h2c_out.valid"<<endl;
	cout<<((axi_lite[512+16]>>27) & 1) << " Report 27:tlb.io.h2c_out.ready"<<endl;
	cout<<((axi_lite[512+16]>>28) & 1) << " Report 28:fifo_h2c_cmd.io.out.valid"<<endl;
	cout<<((axi_lite[512+16]>>29) & 1) << " Report 29:fifo_h2c_cmd.io.out.ready"<<endl;
	cout<<endl;
	cout<<((axi_lite[512+16]>>30) & 1) << " Report 30:io.c2h_cmd.valid"<<endl;
	cout<<((axi_lite[512+16]>>31) & 1) << " Report 31:io.c2h_cmd.ready"<<endl;
	cout<<((axi_lite[512+17]>>0) & 1)  << " Report 32:boundary_split.io.data_out.valid"<<endl;
	cout<<((axi_lite[512+17]>>1) & 1)  << " Report 33:boundary_split.io.data_out.ready"<<endl;
	cout<<((axi_lite[512+17]>>2) & 1)  << " Report 34:fifo_c2h_data.io.out.valid"<<endl;
	cout<<((axi_lite[512+17]>>3) & 1)  << " Report 35:fifo_c2h_data.io.out.ready"<<endl;
	cout<<endl;
	cout<<((axi_lite[512+17]>>4) & 1)  << " Report 36:io.h2c_data.valid"<<endl;
	cout<<((axi_lite[512+17]>>5) & 1)  << " Report 37:io.h2c_data.ready"<<endl;
	cout<<((axi_lite[512+17]>>6) & 1)  << " Report 38:fifo_h2c_data.io.in.valid"<<endl;
	cout<<((axi_lite[512+17]>>7) & 1)  << " Report 39:fifo_h2c_data.io.in.ready"<<endl;
}