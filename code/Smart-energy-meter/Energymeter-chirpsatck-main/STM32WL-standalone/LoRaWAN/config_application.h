#define ACTIVATION_MODE     		OTAA
#define CLASS						CLASS_A
#define SPREADING_FACTOR    		12
#define ADAPTIVE_DR         		true
#define CONFIRMED           		true
#define APP_PORT                	2

#define SEND_BY_PUSH_BUTTON 		false
#define FRAME_DELAY         		15000
#define PAYLOAD_1234				false
#define PAYLOAD_TEMPERATURE    		false
#define PAYLOAD_HUMIDITY   			false
#define CAYENNE_LPP_         		false
#define LOW_POWER           		false



#define RANGE_TESTING_MODE      	false    // Set to true for range/penetration testing


#define devEUI_						{0xB8,0xD5,0xC1,0x6E,0x96,0xB2,0xB6,0x61}//


// Configuration for ABP Activation Mode
#define devAddr_ 					( uint32_t )0x00000000
#define nwkSKey_ 					00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00
#define appSKey_ 					00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00


// Configuration for OTAA Activation Mode
#define appKey_						A5,C0,E6,E5,17,B0,F3,96,27,59,31,39,5D,AD,48,9A//a5c0e6e517b0f396275931395dad489a//60,39,D5,AC,68,A8,59,E9,5F,9E,88,CC,7D,BC,77,47//97,5F,DF,71,EB,A2,14,2B,50,84,AE,84,9C,09,4B,26
#define appEUI_				    	{0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01}
