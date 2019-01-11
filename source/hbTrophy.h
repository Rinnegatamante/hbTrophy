#ifndef __HB_TROPHY_H__
#define __HB_TROPHY_H__

typedef uint32_t hbTrophyContext;
typedef uint32_t hbTrophyHandle;
typedef uint32_t hbTrophyId;

typedef struct hbTrophyCommunicationId {
	char data[9];
	char term;
	uint8_t num;
	char dummy;
} hbTrophyCommunicationId;

typedef struct hbTrophyCommunicationSignature {
	char data[160];
} hbTrophyCommunicationSignature;

#define HB_TROPHY_ERROR_OK                  0x00000000
#define HB_TROPHY_ERROR_ALREADY_INITIALIZED 0x80000001
#define HB_TROPHY_ERROR_NOT_INITIALIZED     0x80000002
#define HB_TROPHY_ERROR_OUT_OF_MEMORY       0x80000003
#define HB_TROPHY_ERROR_TOO_MANY            0x80000004
#define HB_TROPHY_ERROR_INVALID_HANDLE      0x80000005
#define HB_TROPHY_ERROR_INVALID_COMM_ID     0x80000006
#define HB_TROPHY_ERROR_INVALID_TRP_LIST    0x80000007

#define HB_TROPHY_INVALID_TROPHY_ID         -1

int hbTrophyInit(void *opt);
int hbTrophyTerm(void);
int hbTrophyCreateContext(hbTrophyContext *context, hbTrophyCommunicationId *id, hbTrophyCommunicationSignature sign, uint64_t options);
int hbTrophyCreateHandle(hbTrophyHandle *handle);
int hbTrophyDestroyHandle(hbTrophyHandle *handle);

#endif
