#include <memory.h>
#include <string.h>

#ifndef  __CCLOUDINTERFACE_HPP__
#define __CCLOUDINTERFACE_HPP__

#define CCloudBuffer									2000    // record of array
#define RefIdLength	                                20       

#define RequestType_GetOperator				   1
#define ResponseType_GetOperatorAck		   100

#define RequestType_ServiceAuthen			    2
#define ResponseType_ServiceAuthenAck		200

#define RequestType_ChargeInit			            3
#define ResponseType_ChargeInitAck		        300

#define RequestType_ChargeUpdate	            4
#define ResponseType_ChargeUpdateAck		400

#define RequestType_ChargeTerminate	         5
#define ResponseType_ChargeTerminateAck	 500

struct GetOperator
{
	char				refid[RefIdLength];
	char				bNumber[50];
};

struct GetOperatorReply
{
	char				refid[RefIdLength];
	char				bNumber[50];
	char				operName[256];
};

//

struct ServiceAuthen
{
	char				refid[RefIdLength];
	char				aNumber[50];
	char				bNumber[50];
};

struct ServiceAuthenReply
{
	char				refid[RefIdLength];
	char				aNumber[50];
	char				bNumber[50];
	int					flagReserve;
	char              chargeInfo[20];
};

//
struct ChargeInit
{
	char				refid[RefIdLength];
	char				aNumber[50];
	char				bNumber[50];
};

struct ChargeInitReply
{
	char				refid[RefIdLength];
	char				aNumber[50];
	char				bNumber[50];
	int					flagReserve;
};

//
struct ChargeUpdate
{
	char				refid[RefIdLength];
	char				bNumber[50];
};

struct ChargeUpdateReply
{
	char				refid[RefIdLength];
	char				bNumber[50];
	int                 flagReserve;
	int                 iSeconds;
};

//


struct ChargeTerminate
{
	char				refid[RefIdLength];
	char				bNumber[50];
};

struct ChargeTerminateReply
{
	char				refid[RefIdLength];
	char				bNumber[50];
	int                 flagReserve;
};




/*
int  CreatePackage_GetOperator(GetOperator * request, char * buffer, int size)
{
	memset(buffer, 0, size);
	*buffer = 0x02; buffer++;
	int n = strlen(request->refid);
	n += strlen(request->bNumber);
	n + =1;
	*buffer = (char)(n & 0x000000FF); buffer++;
	*buffer = (char)(n & 0x0000FF00); buffer++;
	*buffer = (char)(n & 0x00FF0000); buffer++;
	*buffer = (char)(n & 0xFF000000); buffer++;
	//memcpy(buffer, request->refid, sizeof(request->refid));
	//memcpy(buffer, request->bNumber, sizeof(request->bNumber));
	sprintf(buffer, "%s,%s", request->refid, request->bNumber);
	buffer += n;
	*buffer = 0x03;
	return n + 1 + 1 + 4;
}

*/

struct ArrayElement
{
	char                ifUsed;
	char				request[256];
	int                 reqType;

	char                ifReply;
	char				response[256];
	int                resType;
	ArrayElement():ifUsed('0'),reqType(0),ifReply('0'),resType(0) {
		static char const initValue[] = "";
		strcpy(request,initValue);
		strcpy(response,initValue);	
	}
};


struct ReplyOfAPI
{
	char				refid[20];
	char				response[256];
};
	
#endif 




