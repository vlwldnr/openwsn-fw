/**
\brief An example CoAP application.
*/

#include "opendefs.h"
#include "ctest.h"
#include "opencoap.h"
#include "opentimers.h"
#include "openqueue.h"
#include "packetfunctions.h"
#include "openserial.h"
#include "openrandom.h"
#include "scheduler.h"
//#include "ADC_Channel.h"
#include "idmanager.h"
#include "IEEE802154E.h"

//=========================== defines =========================================

/// inter-packet period (in ms)
#define CTESTPERIOD 10000
#define PAYLOADLEN      40

const uint8_t ctest_path0[] = "test";
const uint8_t ctest_ledpath[] = "l";

// TODO: Change Address..
static const uint8_t ipAddr_targetMote[] = {0xbb, 0xbb, 0x00, 0x00, 0x14, 0x15, 0x92, 0x00, \
                                           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02};

//=========================== variables =======================================

ctest_vars_t ctest_vars;

//=========================== prototypes ======================================

owerror_t ctest_receive(OpenQueueEntry_t* msg,
                    coap_header_iht*  coap_header,
                    coap_option_iht*  coap_options);
void    ctest_timer_cb(opentimer_id_t id);
void    ctest_task_cb(void);
void    ctest_sendDone(OpenQueueEntry_t* msg,
                       owerror_t error);

//=========================== public ==========================================

void cexample_init() {
   
   // prepare the resource descriptor for the /ex path
   cexample_vars.desc.path0len             = sizeof(ctest_path0)-1;
   cexample_vars.desc.path0val             = (uint8_t*)(&ctest_path0);
   cexample_vars.desc.path1len             = 0;
   cexample_vars.desc.path1val             = NULL;
   cexample_vars.desc.componentID          = COMPONENT_CTEST;
   cexample_vars.desc.callbackRx           = &ctest_receive;
   cexample_vars.desc.callbackSendDone     = &ctest_sendDone;
   
   
   opencoap_register(&ctest_vars.desc);
   cexample_vars.timerId    = opentimers_start(CTESTPERIOD,
                                                TIMER_PERIODIC,TIME_MS,
                                                ctest_timer_cb);
}

//=========================== private =========================================

owerror_t ctest_receive(OpenQueueEntry_t* msg,
			coap_header_iht* coap_header,
			coap_option_iht* coap_options){
  return E_FAIL;
}

// Timer Fired
void ctest_timer_cb(opentimer_id_t id){
  scheduler_push_task(ctest_task_cb, TASKPRIO_COAP);
}

void ctest_task_cb(){
  OpenQueueEntry_t*   pkt;
  owerror_t           outcome;

  uint8_t i=0;

  // don't run if not synced
  if(ieee154e_isSynch() == FALSE) return;

  // don't run on dagroot
  if(idmanger_getIsDAGRoot()){
    opentimers_stop(ctest_vars.timerID);
    return;
  }


  // Create CoAP Packet
  pkt = openqueue_getFreePacketBuffer(COMPONENT_CTEST);
  if(pkt == NULL){
    openserial_printError(
			  COMPONENT_CTEST,
			  ERR_NO_FREE_PACKET_BUFFER,
			  (errorparameter_t)0,
			  (errorparameter_t)0
			  );
    openqueue_freePacketBuffer(pkt);
    return;
  }

  // assign ownership
  pkt->creator = COMPONENT_CTEST;
  pkt->owner   = COMPONENT_CTEST;

  // Process i
  i = (++i)%2;

  packetfunctions_reserveHeaderSize(pkt, 1);
  pkt->payload[0] = i;

  packetfunctions_reserveHeaderSize(pkt, 1);
  pkt->payload[0] = COAP_PAYLOAD_MARKER;

   // content-type option
   packetfunctions_reserveHeaderSize(pkt,2);
   pkt->payload[0]                = (COAP_OPTION_NUM_CONTENTFORMAT - COAP_OPTION_NUM_URIPATH) << 4
                                    | 1;
   pkt->payload[1]                = COAP_MEDTYPE_APPOCTETSTREAM;
  
   // Location-path option
   packetfunctions_reserveHeaderSize(pkt, sizeof(ctest_ledpath)-1);
   memcpy(&pkt->payload[0], ctest_ledpath, sizeof(ctest_ledpath)-1);
   packetfunctions_reserveHeadersize(pkt,1);
   packetfunctions_reserveHeaderSize(pkt,1);
   pkt->payload[0]                = ((COAP_OPTION_NUM_URIPATH) << 4) | (sizeof(ctest_ledpath)-1);
     
   // metadata
   pkt->l4_destination_port       = WKP_UDP_COAP;
   pkt->l3_destinationAdd.type    = ADDR_128B;
   memcpy(&pkt->l3_destinationAdd.addr_128b[0],&ipAddr_targetMote,16);
   
   // send
   outcome = opencoap_send(
      pkt,
      COAP_TYPE_NON,
      COAP_CODE_REQ_PUT,
      1,
      &ctest_vars.desc
   );
   
   // avoid overflowing the queue if fails
   if (outcome==E_FAIL) {
      openqueue_freePacketBuffer(pkt);
   }
   
   return;
}

void ctest_sendDone(OpenQueueEntry_t* msg, owerror_t error) {
   openqueue_freePacketBuffer(msg);
}
