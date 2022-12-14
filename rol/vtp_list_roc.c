/*************************************************************************
 *
 *  vtp_list.c -      Library of routines for readout of events using a
 *                    JLAB Trigger Interface V3 (TI) with a VTP in
 *                    CODA 3.0.
 *
 *                    This is for a VTP with serial connection to a TI
 *
 */

/* Event Buffer definitions */
#define MAX_EVENT_LENGTH 40960
#define MAX_EVENT_POOL   100

#include <VTP_source.h>

/* Note that ROCID is a static readout list variable that gets set
   automatically in the ROL initialization at Download. */


#define NUM_VTP_CONNECTIONS 1
#define MAXBUFSIZE 1048576

/* create a User Buffer for events - 4MB*/
unsigned int ubuf[MAXBUFSIZE];


/* define an array of Payload port Config Structures */
PP_CONF ppInfo[16];

int blklevel = 1;
int maxdummywords = 200;

/* Data necessary to connect using EMUSocket
#define CMSG_MAGIC_INT1 0x634d7367
#define CMSG_MAGIC_INT2 0x20697320
#define CMSG_MAGIC_INT3 0x636f6f6c
*/
unsigned int emuData[] = {0x634d7367,0x20697320,0x636f6f6c,6,0,4196352,1,1};


/**
                        DOWNLOAD
**/
void
rocDownload()
{

  int stat;
  char buf[1000];
  /* Streaming firmware files for VTP */
  //  const char *z7file="fe_vtp_vxs_readout_z7_jul28.bin";
  //  const char *z7file="fe_vtp_vxs_readout_z7_oct17.bin"; /*added trigger Ack count*/
  //const char *z7file="fe_vtp_vxs_readout_z7_nov5.bin"; /* added TCP window offset */
  const char *z7file="fe_vtp_vxs_readout_z7_nov16.bin"; /* using updated TCP Stack */

  const char *v7file="fe_vtp_vxs_readout_v7_aug8.bin";


  /* Open VTP library */
  stat = vtpOpen(VTP_FPGA_OPEN | VTP_I2C_OPEN | VTP_SPI_OPEN);
  if(stat < 0)
    {
      printf(" Unable to Open VTP driver library.\n");
    }


  /* Load firmware here */
  sprintf(buf, "/usr/local/src/vtp/firmware/%s", z7file);
  if(vtpZ7CfgLoad(buf) != OK)
    {
      printf("Z7 programming failed... (%s)\n", buf);
    }

  printf("loading V7 firmware...\n");
  sprintf(buf, "/usr/local/src/vtp/firmware/%s", v7file);
  if(vtpV7CfgLoad(buf) != OK)
    {
      printf("V7 programming failed... (%s)\n", buf);
    }


  ltm4676_print_status();

  if(vtpInit(VTP_INIT_CLK_VXS_250))
  {
    printf("vtpInit() **FAILED**. User should not continue.\n");
    return;
  }


  /* Get ROC Output network info from VTP */
  stat = vtpRocReadNetFile(0);
  if(stat != OK) {
    daLogMsg("ERROR","Can't load VTP ROC network interface information from file");
    ROL_SET_ERROR;
  }else{
    printf(" **Info from ROC Network Output File (in /mnt/boot/)**\n");
    printf("   IP1 = 0x%08x\n", VTP_NET_OUT.ip[0]);
    printf("   GW1 = 0x%08x\n", VTP_NET_OUT.gw[0]);
    printf("   SM1 = 0x%08x\n", VTP_NET_OUT.sm[0]);
    printf("   MAC1 =   0x%08x 0x%08x\n", VTP_NET_OUT.mac[0][0], VTP_NET_OUT.mac[1][0]);
    printf("\n");
  }

 /* print some connection info from the ROC */
  printf(" **Info from ROC Connection Structure**\n");
  printf("   ROC Type = %s\n", rol->rlinkP->type);
  printf("   EMU name = %s\n", rol->rlinkP->name);
  printf("   EMU IP   = %s\n", rol->rlinkP->net);
  printf("   EMU port = %d\n", rol->rlinkP->port);
  
  /* Configure the ROC*/
  /*  *(rol->async_roc) = 1; */  // don't send Control events to the EB (Set in VTP_source.h)
  vtpRocReset(0);
  printf(" Set ROC ID = %d \n",ROCID);
  vtpRocConfig(ROCID, 0, 64, 0);  /* Use defaults for other parameters MaxRecSize, Max#Blocks, Timeout*/
  emuData[4] = ROCID;  /* define ROCID in the EB Connection data as well*/

  vtpRocStatus(0);

}

/**
                        PRESTART
**/
void
rocPrestart()
{

  int status;
  unsigned int emuip, emuport;
  int ppmask=0;

  VTPflag = 0;

  printf("calling VTP_READ_CONF_FILE ..\n");fflush(stdout);

  printf("%s: rol->usrConfig = %s\n",
	 __func__, rol->usrConfig);

  /* Read Config file and Intialize VTP */
  vtpInitGlobals();
  if(rol->usrConfig)
    vtpConfig(rol->usrConfig);

  /* Get EB connection info to program the VTP TCP stack */
  emuip = vtpRoc_inet_addr(rol->rlinkP->net);
  //emuip = vtpRoc_inet_addr("10.10.10.1");
  //emuip = 0x81396de8;  // test for DEBUG
  emuport = rol->rlinkP->port;
  //emuport = 46101;  //tmp for ncat testing
  printf(" EMU IP = 0x%08x  Port= %d\n",emuip, emuport);


  /* Soft Reset the VTP to clear any buffers and counters */
  vtpV7SoftReset();

  /* Reset the ROC */
  vtpRocReset(0);

  /* Initialize the TI Interface */
  vtpTiLinkInit();


  /* Setup the VTP 10Gig network registers manually and connect */
  {
    unsigned char ipaddr[4];
    unsigned char subnet[4];
    unsigned char gateway[4];
    unsigned char mac[6];
    unsigned char destip[4];
    unsigned short destipport;

    // VTP IP Address
    ipaddr[0]=129; ipaddr[1]=57; ipaddr[2]=109; ipaddr[3]=132;
	//ipaddr[0]=10; ipaddr[1]=10; ipaddr[2]=10; ipaddr[3]=2;
    // Subnet mask
    subnet[0]=255; subnet[1]=255; subnet[2]=255; subnet[3]=0;
    // Gateway
    gateway[0]=129; gateway[1]=57; gateway[2]=109; gateway[3]=1;
       //gateway[0]=10; gateway[1]=10; gateway[2]=10; gateway[3]=1;
    // VTP MAC
    mac[0]=0xce; mac[1]=0xba; mac[2]=0xf0; mac[3]=0x03; mac[4]=0x00; mac[5]=0xa8;

    /* Set VTP connection registers */
    //vtpRocSetTcpCfg(ipaddr, subnet, gateway, mac, emuip, emuport);

    /* Set TCP Stack Registers - the new way */
    vtpRocSetTcpCfg2(0, emuip, emuport);


    /*Read it back to to make sure */
       vtpRocGetTcpCfg(
          ipaddr,
          subnet,
          gateway,
          mac,
          destip,
          &destipport
      );
       printf(" Readback of TCP CLient Registers:\n");
       printf("   ipaddr=%d.%d.%d.%d\n",ipaddr[0],ipaddr[1],ipaddr[2],ipaddr[3]);
       printf("   subnet=%d.%d.%d.%d\n",subnet[0],subnet[1],subnet[2],subnet[3]);
       printf("   gateway=%d.%d.%d.%d\n",gateway[0],gateway[1],gateway[2],gateway[3]);
       printf("   mac=%02x:%02x:%02x:%02x:%02x:%02x\n",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
       printf("   destip=%d.%d.%d.%d\n",destip[0],destip[1],destip[2],destip[3]);
       printf("   destipport=%d\n",destipport);


      /* Make the Connection. Pass Data needed to complete connection with the EMU */
       status = vtpRocTcpConnect(1,emuData,8);
  }

  
  /* Reset and Configure the MIG and ROC Event Builder */
  vtpRocMigReset();
  

  /* Program Payload Port information - All FADC boards, bonded lanes, build to bank 1*/
  // vtpPayloadConfig(3,ppInfo,1,1,0x01);
  vtpPayloadConfig(6,ppInfo,1,1,0x01);
  ppmask = vtpPayloadConfig(12,ppInfo,1,1,0x01);
  // vtpPayloadConfig(7,ppInfo,1,1,0x01);
  // vtpPayloadConfig(9,ppInfo,1,1,0x01);
  //  ppmask = vtpPayloadConfig(13,ppInfo,1,1,0x01);

  /* Example for payload port 8  hosting 4 MPD boards, building to bank 2 */
  //  vtpPayloadConfig(8,ppInfo,2,0,0x02020202);


  printf("vtpPayloadConfig ppmask = 0x%04x\n",ppmask);

  /* Initialize and program the ROC Event Builder*/
  vtpRocEbStop();
  vtpRocEbInit(5,6,7);   // define bank1 tag = 5, bank2 tag = 6, bank3 tag = 7
  vtpRocEbConfig(ppInfo,0);  // blocklevel=0 will skip setting the block level


  /* Reset the data Link between V7 ROC EB and the Zync FPGA ROC */  
  vtpRocEbioReset();
  

  /* Set TI readout to Hardware mode */
  vtpTiLinkSetMode(1);

  /* Enable Async&EB Events for ROC   bit2 - Async, bit1 - Sync, bit0 V7-EB */
  vtpRocEnable(0x5);


  /* Print Run Number and Run Type */
  printf(" Run Number = %d, Run Type = %d \n",rol->runNumber,rol->runType);

  /*Send Prestart Event*/
  status = vtpRocEvioWriteControl(EV_PRESTART,rol->runNumber,rol->runType);
  if(status == ERROR) {
    /* We were unable to send the Prestart Event so we cannot complete the transition
       set the error condition to tell the ROC */
    ROL_SET_ERROR;
  }


  /* Send a User Event - read in a File */
  //  vtpRocFile2Event("/daqfs/home/abbottd/test.txt",(unsigned char *)&ubuf[0],130, ROCID, (MAXBUFSIZE<<2));
  vtpRocFile2Event("/daqfs/daq_setups/vtp-mpdro/cfg/pedestals/daq_ped_bb_gem_run10975.dat",
		   (unsigned char *)&ubuf[0],130, ROCID, (MAXBUFSIZE<<2));
  printf(" User Event header = 0x%08x 0x%08x 0x%08x 0x%08x\n", ubuf[0], ubuf[1], ubuf[2], ubuf[3]);
  status = vtpRocEvioWriteUserEvent(ubuf);
  if(status != OK) {
    printf(" rocPrestart: ERROR: vtpRocEvioWriteUserEvent() failed\n"); 
  }


  printf(" Done with User Prestart\n");

}

/**
                        PAUSE
**/
void
rocPause()
{
  CDODISABLE(VTP, 1, 0);
}

/**
                        GO
**/
void
rocGo()
{
  int chmask = 0;

  /* Clear TI Link recieve FIFO */
  vtpTiLinkResetFifo(1);


  chmask = vtpSerdesCheckLinks();
  printf("VTP Serdes link up mask = 0x%05x\n",chmask);

  printf("Calling vtpSerdesStatusAll()\n");
  vtpSerdesStatusAll();

  /* Get the current Block Level from the TI */
  blklevel = vtpTiLinkGetBlockLevel(0);
  printf("\nBlock level read from TI Link = %d\n", blklevel);


  /* Update the ROC EB blocklevel in the EVIO banks */
  vtpRocEbSetBlockLevel(blklevel);


  /* Start the ROC Event Builder */
  vtpRocEbStart();

  /*Send Go Event*/
  vtpRocEvioWriteControl(EV_GO,0,*(rol->nevents));


  /* Enable with val = 7 to recieve Block Triggers or 5 to run without software intervention */
  CDOENABLE(VTP, 1, 5);

}


/**
                        END
**/
void
rocEnd()
{
  int status;
  unsigned int ntrig;
  unsigned long long nlongs, nbytes;

  CDODISABLE(VTP, 1, 0);

  /* Get total event information and set the Software ROC counters */
  ntrig = vtpRocGetTriggerCnt();
  *(rol->nevents) = ntrig;
  *(rol->last_event) = ntrig;


  /*Send End Event*/
  vtpRocEvioWriteControl(EV_END,rol->runNumber,*(rol->nevents));


  /* Disable the ROC EB */
  vtpRocEbStop();


  vtpRocStatus(0);

  /* Disconnect the socket */
  status = vtpRocTcpConnect(0,0,0);
  if(status == ERROR) {
    printf("rocEnd: Error closing socket\n");
  }


  /* Print final Stats */
  nlongs = vtpRocGetNlongs();
  *(rol->totalwds) = nlongs;
  vtpRocGetNBytes(&nbytes);
  printf(" TOTAL Triggers = %d   Nlongs = %lld (0x%llx Bytes)\n",ntrig, nlongs, nbytes);

}

/**
                        READOUT
**/
void
rocTrigger(int EVTYPE)
{

  int ii, ntrig, ntack=0;

/* Right now this is a mostly dummy routine as the trigger and readout is
   running in the FPGAs. In principle however the ROC can poll on
   a register holding the number of unacknowledged readout triggers which will allow it 
   to enter this routine and the User can insert an asynchonous event into the data stream. 

   Also the ROC can require that every trigger block is managed by this routine.
   Essentially, one is forcing the FPGA to get an acknowledge of the trigger
   by the software ROC.

*/

  ntack = vtpRocPoll();   /* check how many triggers need to be acknowledged */

  /* Get total event information and update the Software ROC counters */
  ntrig = vtpRocGetTriggerCnt();
  *(rol->nevents) = ntrig;
  *(rol->last_event) = ntrig;

  //  printf("%s: In trigger routine: %d triggers being acknowledged\n",__func__,ntack);
  
  for(ii=0;ii<ntack;ii++) {
    vtpRocWriteBank(NULL);     // Write a NULL bank to just acknowledge the trigger
  }


}

/**
                        READOUT ACKNOWLEDGE
**/
void
rocTrigger_done()
{
  CDOACK(VTP, 1, 0);   /* This does nothing right now - DJA */
}

/**
                        RESET
**/
void
rocReset()
{

  /* Force Disconnect of the socket */
  vtpRocTcpConnect(-1,0,0);  


  /* Close the VTP Library */
  vtpClose(VTP_FPGA_OPEN|VTP_I2C_OPEN|VTP_SPI_OPEN);
}

/*
  Local Variables:
  compile-command: "make -k vtp_list.so"
  End:
 */
