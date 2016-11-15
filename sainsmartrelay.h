#ifndef sainsmartrelay_h
#define sainsmartrelay_h

#define VENDOR_ID 0x0403
#define DEVICE_ID 0x6001

#define FIRST_RELAY    1
#define MAX_NUM_RELAYS 4
#define MAX_RELAY_CARD_NAME_LEN 40
#define MAX_COM_PORT_NAME_LEN 32

typedef unsigned char  uint8;
typedef unsigned short uint16;
typedef unsigned long  uint32;

typedef enum
{
   OFF=0,
   ON,
   PULSE,
   INVALID
}
relay_state_t;

typedef enum
{
    ID_OFF_ALL = 0,
    ID_ON,
    ID_ON_MULTIPLE,
    ID_ON_ALL,
    ID_OFF,
    ID_OFF_MULTIPLE,
    ID_GET,
    ID_GET_ALL
}
operations;

#endif
