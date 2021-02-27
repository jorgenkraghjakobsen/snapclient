#ifndef _PROTOCOL_H_ 
#define _PROTOCOL_H_

extern xQueueHandle prot_queue;
void protocolHandlerTask(void *pvParameter);

#endif /* _PROTOCOL_H_  */ 
