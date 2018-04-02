#include "precomp.h"
#include "ntstrsafe.h"
#include "analysispacket.h"


typedef struct in_addr {
	union {
		struct { UCHAR s_b1,s_b2,s_b3,s_b4; } S_un_b;
		struct { USHORT s_w1,s_w2; } S_un_w;
		ULONG S_addr;
	} S_un;
} IN_ADDR, *PIN_ADDR, FAR *LPIN_ADDR;

typedef struct IP_HEADER
{
	unsigned char  VIHL;          // Version and IHL
	unsigned char  TOS;           // Type Of Service
	short          TotLen;        // Total Length
	short          ID;            // Identification
	short          FlagOff;       // Flags and Fragment Offset
	unsigned char  TTL;           // Time To Live
	unsigned char  Protocol;      // Protocol
	unsigned short Checksum;      // Checksum
	struct in_addr        iaSrc;  // Internet Address - Source
	struct in_addr        iaDst;  // Internet Address - Destination
}IP_HEADER, *PIP_HEADER;

typedef struct TCP_HEADER
{
	unsigned short SrcPort;
	unsigned short DestPort;
	ULONG          SeqNum;
	ULONG          AckNum;
	unsigned short LenAndRes;
	unsigned short WindowSize;
	unsigned short CheckKey;
	unsigned short UrgentPointer;
}TCP_HEADER, *PTCP_HEADER;

#define IP_OFFSET                               0x0E

//IP Protocol Types
#define PROT_ICMP                               0x01 
#define PROT_TCP                                0x06 
#define PROT_UDP                                0x11 

NDIS_SPIN_LOCK                     gWaitingSpinLock;//����ְ�ʱ��������
WAITING_INFO                        WaitingInfo[100];
int                                 WaitingCnt;
DEAL_INFO                          DealInfos[300];
int                                count;
URL_INFO													 UrlInfos[100]={0};
int																 urlCnt = 0;

//win7��ֹ������վ
DWORD dw_win7UrlCnt = 4;
char *psz_win7DangeUrl[]={"blfz.net","www.blwg.net","blwg.net","jsqjsq.com"};
// ���������
//	Packet�� ��������NDIS��������
//	bRecOrSend: ����ǽ��հ���ΪTRUE;���Ϊ���Ͱ���ΪFALSE��
// ����ֵ��
//	���������£�������ͨ������ֵ�Ծ�����δ���NDIS����������ʧ�ܡ�ת��
FILTER_RESULT AnalysisPacket(PNDIS_PACKET Packet,  BOOLEAN bRecOrSend)
{
	FILTER_RESULT status = STATUS_PASS; // Ĭ��ȫ��ͨ��
	PNDIS_BUFFER NdisBuffer ;
	UINT TotalPacketLength = 0;
	UINT copysize = 0;
	UINT DataOffset = 0 ;
	UINT PhysicalBufferCount;
	UINT BufferCount   ;
	PUCHAR pPacketContent = NULL;
	PUCHAR pPacketContentTmp = NULL;
	char* tcsPrintBuf = NULL;
	PUCHAR tembuffer = NULL ; 
	UINT j;

	__try{

    
		status = NdisAllocateMemoryWithTag( &pPacketContent, 2048, TAG); 
		if( status != NDIS_STATUS_SUCCESS ){
			status = NDIS_STATUS_FAILURE ;
			__leave;
		}
		pPacketContentTmp = pPacketContent;
		NdisZeroMemory( pPacketContent, 2048 ) ;
		

		// �ҵ���һ��Ndis_Buffer��Ȼ��ͨ��ͨ��NdisGetNextBuffer����ú�����NDIS_BUFFER��
		// ���ֻ���ҵ�һ���ڵ㣬�����ҷ���ķ����ǵ���NdisGetFirstBufferFromPacket��
		NdisQueryPacket(Packet,  // NDIS_PACKET        
			&PhysicalBufferCount,// �ڴ��е��������
			&BufferCount,		 // ���ٸ�NDIS_BUFFER��
			&NdisBuffer,         // �����ص�һ����
			&TotalPacketLength	 // �ܹ��İ����ݳ���
			);
		if( TotalPacketLength > 2048 )
		{
			status = NDIS_STATUS_FAILURE ;
			__leave;
		}
		while(TRUE){

			// ȡ��Ndis_Buffer�д洢�������������ַ��
			// �����������һ���汾��NdisQueryBuffer��
			// ������ϵͳ��Դ�ͻ��������ľ���ʱ�򣬻����Bug Check������������h
			NdisQueryBufferSafe(NdisBuffer,
				&tembuffer,// ��������ַ
				&copysize, // ��������С 
				NormalPagePriority
				);

			// ���tembufferΪNULL��˵����ǰϵͳ��Դ�ѷ���
			if(tembuffer != NULL){
				NdisMoveMemory( pPacketContent + DataOffset , tembuffer, copysize) ;			
				DataOffset += copysize;
			}

			// �����һ��NDIS_BUFFER��
			// ����õ�����һ��NULLָ�룬˵���Ѿ�������ʽ��������ĩβ�����ǵ�ѭ��Ӧ�ý����ˡ�
			NdisGetNextBuffer(NdisBuffer , &NdisBuffer ) ;

			if( NdisBuffer == NULL )
				break ;
		}

		// ȡ�����ݰ����ݺ����潫�������ݽ��й��ˡ�
		// ��������������е�ʵ�֣������򵥵ش�ӡһЩ�ɶ���Log��Ϣ��
		if(pPacketContent[12] == 8 &&  pPacketContent[13] == 0 )  //is ip packet
		{	
			PIP_HEADER pIPHeader = (PIP_HEADER)(pPacketContent + IP_OFFSET);
			switch(pIPHeader->Protocol)
			{
			case PROT_ICMP:
				if(bRecOrSend)
					KdPrint(("Receive ICMP packet"));
				else
					KdPrint(("Send ICMP packet"));

				//
				// ȡ��ICMPͷ��������Ĺ����жϡ�
				// 
				break;
			case PROT_UDP:
				
				if(bRecOrSend)
					KdPrint(("Receive UDP packet"));
				else
					KdPrint(("Send UDP packet"));

				//
				// ȡ��UDPͷ��������Ĺ����жϡ�
				//
				
				break;
			case PROT_TCP:
				if(bRecOrSend)
				{
					KdPrint(("Receive TCP packet"));
					break;
					
				}
				else
				{//send packet
					
					PTCP_HEADER pTcpHeader = (PTCP_HEADER)( (UCHAR*)pIPHeader + ((pIPHeader->VIHL)&0xf)*4 );
					UINT HeaderLen = (((pTcpHeader->LenAndRes)&0x00f0)>>4)*4 + ((pIPHeader->VIHL)&0xf)*4 + 0xe;
				
					
					if( (DataOffset > HeaderLen) && (pTcpHeader->DestPort == 0x5000) )
					{//��Ӧ�ò����ݣ���Ӧ�ó̵Ķ˿ں�Ϊ80
						char *pAppData = pPacketContent + HeaderLen;
						int AppDataLen = DataOffset - HeaderLen;
						
						//��Ӧ�ò㽻���������
						DEAL_INFO DealInfo = {0};
						PKEVENT pKEvent = NULL;
						UNICODE_STRING EventName;
						WCHAR ch[200];
						HANDLE hThread;
						UINT index = 0;
						
						//�������ݰ�SeqNum���ж��Ƿ��Ǳ��ȴ���
						UINT WaitingIndex = 0;
						
						//�洢��ȡ�����ַ
						char pRealUrl[2048] = {0};
						UINT len = 0;
						
						
						//�洢��ȡ���refer�����ڼ�����
						char pRefer[100] = {0};
						UINT  ReferMainLen = 0;
						
						
						//�����ֽ����������ֽ���ת��
						pTcpHeader->SeqNum = Myhtonl( pTcpHeader->SeqNum );
						
						NdisAcquireSpinLock( &gWaitingSpinLock );
						if( IsWaitingPacket(pTcpHeader->SeqNum, &WaitingIndex) )
						{//�ǵȴ��ְ�
							/*
							������:
							1.����SeqNum�ҵ��洢��WaitingInfo�����ж�Ӧ�������ȡHost���ְ�������state��
							2.����state������ֱ���д������ڶ��ְ��е�Hostֵ��Ϣ��ȡ����
							3.����ȡ���ĵڶ��ְ�Host��Ϣ�������WaitingInfo�еĵ�һ�ְ�Host��Ϣ��ϣ�ƴ�ճ�������Hostֵ��
							  �����ݸ�ֵ���ȣ�����������
							4.����õ�����Host��Ϣ��Url���ϣ�ƴ�ճ�������ַ:Host+Url;�����ݳ��ȣ�����������,
							  ��ֵַ�����pRealUrl������len;
							5.����goto�����ת������������ͬ���Ĵ���������
							*/
							int state = WaitingInfo[WaitingIndex].state;

							NdisReleaseSpinLock( &gWaitingSpinLock );
							//������GET����ֳ�3��/3�����ϰ������
							{
							
							char* pHost = NULL;
							UINT   begin = 0;
							UINT   end   = 0;
							switch( state )
							{
								case 1:
									{//δ��������
										int index = 0;
										begin = 0;
										for(; index < AppDataLen; index++ )
										{
											if( 0 == strncmp( pAppData+index, "\r\n", 2 ) )
											{//��Ϊδ����GET����3�����Ϸְ������Ա�Ȼ�ɹ�
												end = index;
											  break;
											}
										}
										break;
									}
								case 2:
									{//�������ּ���
										int index = 0;
										BOOLEAN bFound = FALSE;
										for(; index < AppDataLen; index++ )
										{
											if( !bFound )
											{
											   if( 0x20  == pAppData[index] )
											   {
												    begin = index+1;
												    bFound = TRUE;
												    continue;
										     }
										  }
											if( 0 == strncmp( pAppData+index, "\r\n", 2 ) )
											{
												end = index;
												break;
											}
										}
										break;
									}
								case 3:
									{//����ȫ������
										int index = 0;
										BOOLEAN bFound = TRUE;
										
										for(; index < AppDataLen; index++ )
										{
											if( !bFound )
										  {
											   if( 0 == strncmp( pAppData+index, "Host:", 5 ) )
											   {
											     	begin = index+6;
												    index = index+5;
												    continue;
											   }
										  }
										  if( 0 == strncmp( pAppData+index, "\r\n", 2 ) )
										  {
										  	end = index;
										  	break;
										  }	
										}
										break;
									}
									
							}
							//���ҵ�ʣ���Host��Ϣ����ʼλ�ã�δ���Ƿְ��� >= 3�������Ȼ����
							if( end > begin )
							{
								//�洢ʣ���Host��Ϣ
								WAITING_INFO WaitingFstInfo = {0};
								int SecHostLen = end - begin;
								pHost = (char*)ExAllocatePool( NonPagedPool, SecHostLen+1 );
								memset( pHost, 0, SecHostLen+1 );
								strncpy( pHost, pAppData+begin, SecHostLen );
								
								//����ȡ��ʣ��Host��Ϣ��ǰ����Host��Ϣ����
								NdisAcquireSpinLock( &gWaitingSpinLock );
								//���»�ȡWaitingIndex
								WaitingIndex = 0;
								if( IsWaitingPacket(pTcpHeader->SeqNum, &WaitingIndex) )
								{//��Ȼ���ҵ�
	                /*
									int HostLen = WaitingInfo[WaitingIndex].HostLen + len;
									//�������Host����󳤶�Ϊ2048����������ֻȡǰ204���ֽ�
									if( HostLen > 2048 )
										HostLen = 2048;
									//����Hostֵ
									strncat( WaitingInfo[WaitingIndex].pHost+WaitingInfo[WaitingIndex].HostLen, pHost, len );
									
									WaitingInfo[WaitingIndex].HostLen = HostLen;
									*/
									
									//�洢����Hostֵ����
									UINT HostLen = 0;
									UINT UrlLen = 0;
									UINT FstHostLen = 0;
									
									WaitingFstInfo = WaitingInfo[WaitingIndex];
									HostLen = WaitingFstInfo.HostLen + SecHostLen;
									UrlLen = WaitingFstInfo.UrlLen;
									FstHostLen = WaitingFstInfo.HostLen;
	                if( HostLen > 2048 )
	                {
	                	HostLen = 2048;
	                }
	                
	
	                WaitingInfo[WaitingIndex].HostLen = HostLen; 
									NdisReleaseSpinLock( &gWaitingSpinLock );
									
	                //����Hostֵ
	                strncat( WaitingFstInfo.pHost+FstHostLen, pHost, HostLen-FstHostLen );
	                ExFreePool( pHost );
	                pHost = NULL;
	                
	                
	                //�����Ϻ��Host����Ҫ��������������Լ������ļ�����ֱ�ӷŹ�
	                if( HostLen > strlen("reputation.cloudsvc.net") )
	                {
	                	if( 0 == strncmp( WaitingFstInfo.pHost, "reputation.cloudsvc.net", strlen("reputation.cloudsvc.net") ) )
	                	{//���������ֻ����������ǵļ�����Hostֵ�������
	                		ExFreePool( WaitingFstInfo.pHost );
					  	        ExFreePool( WaitingFstInfo.pUrl );
					  	        ExFreePool( WaitingFstInfo.pRefer );
	                		
							        return STATUS_PASS;
							      }
	                }
	                //������ַ���������pRealUrl
	                len = HostLen + WaitingFstInfo.UrlLen;
							  	if( len > 2048 )
							  	{//��ַ���ȳ���
							  		if( (len-UrlLen) < 2048 )//�����򳤶Ⱥϸ�ֻ�ͼ�������
							  		{
							  			strncpy( pRealUrl, WaitingFstInfo.pHost, len-UrlLen );
							  			len = len-UrlLen;
							  		}
							  		else//�����򳤶�Ҳ����,ȡHost�򣬵�һ����/��ǰ��ֵ
							  		{
							  			UINT index = 0;
							  			for(; index < len-UrlLen; index++ )
							  			{
							  				if( WaitingFstInfo.pHost[index] == '/' )
							  					break;
							  			}
							  			strncpy( pRealUrl, WaitingFstInfo.pHost, index+1 );
							  			len = index+1;
							  		}
							  	}
							  	else
							  	{//��ַ������
							  		strncpy( pRealUrl, WaitingFstInfo.pHost, len-UrlLen );
							  		strncat( pRealUrl, WaitingFstInfo.pUrl, UrlLen );
							  	}
							  	//��ȡRefer����ַ
							  	if( WaitingFstInfo.ReferLen > 0 )
							  	{
							  		//Refer:http://xxx.xxx,ǰ����7�����÷���
							  		UINT index = 7;
							  		for(; index < WaitingFstInfo.ReferLen; index++ )
							  		{
							  			if( WaitingFstInfo.pRefer[index] == '/' )
							  				break;
							  		}
							  		if( index < WaitingFstInfo.ReferLen )
							  			ReferMainLen = index-7;
							  		else
							  			ReferMainLen = WaitingFstInfo.ReferLen;
							  		if( ReferMainLen < 100 )
							  		  strncpy( pRefer, WaitingFstInfo.pRefer+7, ReferMainLen );
							  		else
							  			KdPrint(("ReferMainLen > 100\n"));
							  	}
									goto Position;
									/*
									//�ְ���������󣬵ڶ��ְ��¼�������
									//���»�ȡWaitingIndex
									WaitingIndex = 0;
									NdisAcquireSpinLock( &gWaitingSpinLock );
									if( IsWaitingPacket(pTcpHeader->SeqNum, &WaitingIndex) )
									{//��Ȼ�ҵ�
										BOOLEAN bSafe = WaitingInfo[WaitingIndex].bSafe;
										for(; WaitingIndex < WaitingCnt; WaitingIndex++ )
										    WaitingInfo[WaitingIndex] = WaitingInfo[WaitingIndex+1];
										NdisReleaseSpinLock( &gWaitingSpinLock );
										
										//�ͷŷ���Ĵ洢ʣ��Host��Ϣ���ڴ�
									  ExFreePool( pHost );
										
										if( bSafe )
											return STATUS_PASS;
										else
											return STATUS_DROP;	
									}
									NdisReleaseSpinLock( &gWaitingSpinLock );								
								}
								NdisReleaseSpinLock( &gWaitingSpinLock );
										
							}
							//δ������ⷵ��
							//������״��
							if( pHost != NULL )
								ExFreePool( pHost );
							return STATUS_PASS;
							*/
						    }
						  }
							
						  }
						}
						else
						   NdisReleaseSpinLock( &gWaitingSpinLock );
						
						//���Ǳ��ȴ��ְ�,�ж��Ƿ�ΪGet�����
						if( IsHttpGetMethod(pAppData, AppDataLen ) )
						{//client������ҳ

							HTTP_INFO HttpUrl = {0};
							HTTP_INFO HttpVersion = {0};
							HTTP_INFO HttpReferer = {0};
							HTTP_INFO HttpHost = {0};
							

							
							int HostLen = 0;
							int UrlLen = 0;
							int ReferLen = 0;
							
							
							//��ʶ�÷ְ��ܷ���ȡ����ɵ�"����"+��ֵ,0:������1:"����"��������ֵ��������2��"����"��������3����
							int state = 0;
							
							//��ȡUrl��Ϣֵ
							HttpUrl = GetHttpGetMethodUrl( pAppData, AppDataLen );
						  //�жϸİ��Ƿ����HTTPЭ���ʽ
						  if( HttpUrl.end_offset < HttpUrl.begin_offset )
							{//Url�����򲻴���Url
								 return STATUS_DROP;
							}
							
							//��ȡHTTP�汾ֵ
							HttpVersion = GetHttpVersion( pAppData, HttpUrl, AppDataLen );
							if( HttpVersion.end_offset < HttpUrl.begin_offset )
								return STATUS_DROP;
							
							HttpReferer = GetHttpSubKey( pAppData, HttpVersion, "Referer:", AppDataLen, &state );
							HttpHost = GetHttpSubKey( pAppData, HttpVersion, "Host:", AppDataLen, &state );
							
							//��ȡ���������ַ
							if( HttpHost.end_offset == 0 )
								HostLen = 0;
							else
							{
							  HostLen = (HttpHost.end_offset - HttpHost.begin_offset+1);
							  HostLen = HostLen < 2048 ? HostLen: 2048;
							}
							if( HttpUrl.end_offset == 0 )
								UrlLen = 0;
							else
							{
							  UrlLen = HttpUrl.end_offset-HttpUrl.begin_offset+1;
							  UrlLen = UrlLen < 2048 ? UrlLen : 2048;
							}
							if( HttpReferer.end_offset == 0 )
								ReferLen = 0;
							else
							{
								ReferLen = HttpReferer.end_offset - HttpReferer.begin_offset+1;
								ReferLen = ReferLen < 2048 ? ReferLen : 2048;
							}
								
							//��������ǹ���ķְ���ֱ�ӷŹ�
							if( HostLen > 0 )
							{
								int StrLen = strlen("reputation.cloudsvc.net");
								int CmpLen = (StrLen > HostLen ? HostLen:StrLen);
								if( 0 == strncmp( pAppData+HttpHost.begin_offset, "reputation.cloudsvc.net", CmpLen ) )
							        return STATUS_PASS;
							  
							}
							
						
							//����ְ��в�����������Host��Ϣ
							if( state != 0 )
							{
								/*
								��������
								1.��ȡHost��Url��Refer��Ϣ�������ݳ���������;
								2.���ݵ�ǰ����SeqNumֵ��������һ�ְ���SeqNumֵ;
								3.��������ȡ����Ϣ����һ�ְ�ֵ���ְ�����state�����浽ȫ������WaitingInfo;
								4.ֱ�ӷŹ������ݰ�
								*/
								//��ʼ���÷ְ���Ϣ
								WAITING_INFO WaitingFstInfo = {0};
								
								//return STATUS_PASS;
								
								WaitingFstInfo.pHost = (char*)ExAllocatePool( NonPagedPool, 2048 );
								memset( WaitingFstInfo.pHost, 0, 2048 );
								if( HostLen > 0 )
								{
									memcpy( WaitingFstInfo.pHost, pAppData+HttpHost.begin_offset, HostLen );
									WaitingFstInfo.HostLen = HostLen;
								}
								
								WaitingFstInfo.pUrl = (char*)ExAllocatePool( NonPagedPool, 2048 );
								memset( WaitingFstInfo.pUrl, 0, 2048 );
								if( UrlLen > 0 )
								{
									memcpy( WaitingFstInfo.pUrl, pAppData+HttpUrl.begin_offset, UrlLen );
									WaitingFstInfo.UrlLen = UrlLen;
							  }
							  
							  WaitingFstInfo.pRefer = (char*)ExAllocatePool( NonPagedPool, 2048 );
							  memset( WaitingFstInfo.pRefer, 0, 2048 );
							  if( ReferLen > 0 )
							  {
							  	memcpy( WaitingFstInfo.pRefer, pAppData+HttpReferer.begin_offset, ReferLen );
							  	WaitingFstInfo.ReferLen = ReferLen;
							  }
							  
							  WaitingFstInfo.SeqNum = pTcpHeader->SeqNum + AppDataLen;
							  if( (pTcpHeader->LenAndRes)&0x0300 )
							  	WaitingFstInfo.SeqNum++;
							  WaitingFstInfo.state = state;

							  
							  //�洢�÷ְ���Ϣ
							  NdisAcquireSpinLock( &gWaitingSpinLock );
							  WaitingInfo[WaitingCnt++] = WaitingFstInfo;
							  NdisReleaseSpinLock( &gWaitingSpinLock );
							  
							  return STATUS_PASS;
							  
							  /*
							  //�ȴ��ְ�����
							  KeWaitForSingleObject( &kFstEvent, Executive, KernelMode, 0, 0 );
							  
							  //������ַ
							  
							  NdisAcquireSpinLock( &gWaitingSpinLock );
							  WaitingIndex = 0;
							  if( IsWaitingPacket(pTcpHeader->SeqNum+AppDataLen, &WaitingIndex) )
							  {//���������Ӧ�óɹ�
							  	 len = WaitingInfo[WaitingIndex].UrlLen + WaitingInfo[WaitingIndex].HostLen;
							  	 NdisReleaseSpinLock( &gWaitingSpinLock);
							  }
							  else 	  //���������
							     NdisReleaseSpinLock( &gWaitingSpinLock);
							    
							  if( len > 0 )
							  {//ֻ�ǶԲ����Ƿ�ɹ��ļ�⣬�������ж�
							  	if( len > 2048 )
							  	{//��ַ���ȳ���
							  		if( (len-UrlLen) < 2048 )//�����򳤶Ⱥϸ�ֻ�ͼ�������
							  		{
							  			strncpy( pRealUrl, WaitingFstInfo.pHost, len-UrlLen );
							  			len = len-UrlLen;
							  		}
							  		else//�����򳤶�Ҳ����,ȡHost�򣬵�һ����/��ǰ��ֵ
							  		{
							  			int index = 0;
							  			for(; index < len-UrlLen; index++ )
							  			{
							  				if( WaitingFstInfo.pHost[index] == '/' )
							  					break;
							  			}
							  			strncpy( pRealUrl, WaitingFstInfo.pHost, index+1 );
							  			len = index+1;
							  		}
							  	}
							  	else
							  	{//��ַ������
							  		strncpy( pRealUrl, WaitingFstInfo.pHost, len-UrlLen );
							  		strncat( pRealUrl, WaitingFstInfo.pUrl, UrlLen );
							  	}
							  }	
							  
							  ExFreePool( WaitingFstInfo.pHost ); 
							  ExFreePool( WaitingFstInfo.pUrl ); 
							  */
							}
							else//�ְ��а���������Host��Ϣ
							{		
						  	len = (HttpHost.end_offset-HttpHost.begin_offset+1)+(HttpUrl.end_offset-HttpUrl.begin_offset+1);
							  if( len > 2048 )
							  {
							  	if( HostLen < 2048 )//ȡ������
							  	{
							  		HTTP_INFO hp = {0};
							  		GetRealUrl( pRealUrl, HttpHost, hp, 2048, pAppData );
							  		len = HostLen;
							  	}
							  	else//ȡ�������е�һ��'\'ǰ��ֵ
							  	{
							  		int index = 0;
							  		for(; index < HostLen; index++ )
							  		{
							  			if( pAppData[HttpHost.begin_offset+index] == '/' )
							  				break;
							  		}
							  		strncpy( pRealUrl, pAppData+HttpHost.begin_offset, index+1 );
							  		len = index+1;
							  	}
								  
							  }
							  else
							     GetRealUrl( pRealUrl, HttpHost, HttpUrl, 2048, pAppData );
							  
							//��ȡRefer����ַ
							  if( ReferLen > 0 )
							  {
								   int index = HttpReferer.begin_offset+7;
								   for(; index-HttpReferer.begin_offset < ReferLen; index++ )
								   {
									    if( pAppData[index] == '/' )
										     break;
							     }
								   if( index-HttpReferer.begin_offset < ReferLen )
									    ReferMainLen = index - HttpReferer.begin_offset - 7;
								   else
									    ReferMainLen = ReferLen;
								   if( ReferMainLen < 100 )
								      strncpy( pRefer, pAppData+HttpReferer.begin_offset+7, ReferMainLen );
								   else
									    KdPrint(("ReferMainLen > 100\n"));
						  	}
						  }
							
							//��Ҫ������ַ����pRealUrl,����Ϊlen
		Position:
							KdPrint(("Url��%s\n", pRealUrl ));
							KdPrint(("Refer��%s\n", pRefer));
							//if( strstr( pRealUrl, "www.163.com") != NULL )
							//	return STATUS_DROP; 
							if( urlCnt > 0 )
							{
								//��������ֹ��url�б�
								int r = 0;
								for( r = 0; r < urlCnt; r++ )
								{
									if( strstr( pRealUrl, UrlInfos[r].pUrl ) != NULL )
										return STATUS_DROP;
								} 
							}
							return STATUS_PASS;
							/*
							//��Ԥ����ֻҪRefer����ַ������ַ��ͬ����ֱ�ӷŹ�
							if( ReferMainLen != 0 && 0 == strncmp( pRealUrl, pRefer, ReferMainLen ) )
							{
								DbgPrint("host and refer are the same, pass\n");
								return STATUS_PASS;
							}
							else
							{
								//���еڶ���Ԥ����,��ȡ��ַ�е�����ַ�����ұ���İ�ȫ����ַ������������ڰ�ȫʱ����
								//��Ϊ��ȫ����ַ��ֱ�ӷŹ������޸ĸ���ַ��Ӧ��ʱ��ֵ
								BOOLEAN bExist = FALSE;
								int MainRealUrlLen = 0;
								char* position = strchr( pRealUrl, '/' );
								if( position != NULL )
								  MainRealUrlLen = position - pRealUrl + 1;
								else
									MainRealUrlLen = len;
								bExist = ExistMainUrl( pRealUrl, MainRealUrlLen );
								if( bExist == TRUE )
								{
									DbgPrint("host is in the safe urls\n");
									return STATUS_PASS;
								}
									
							}
							*/
					  	//��������Ԥ�������Ȼ�޷�ȷ��Ϊ��ȫ��ҳ���������������
					  	/*
					  	��������
					  	1.���۸ð��Ƿ񱻲�֣��������Ҫ������ַ�Ѵ����pRealUrl��
					  	2.���������¼������ڵȴ�Ӧ�ò�Ĵ�����
					  	3.����Ҫ��Ϣ�������ȫ������DealInfos����
					  	4.�ȴ�Ӧ�ò�Ĵ�����
					  	5.�ȴ��¼�������󣬴���DealInfos��WaitingInfo����
					  	6.���ݼ�����������Ƿ�Ź��ð�
					  	*/
					  	
							DealInfo.UrlInfo.pUrl = (char*)ExAllocatePool( NonPagedPool, len+1 );
							memset( DealInfo.UrlInfo.pUrl, 0, len+1 );
							
							memcpy( DealInfo.UrlInfo.pUrl, pRealUrl, len );
							DealInfo.UrlInfo.len = len;
							//ExFreePool(pUrlInfo);
							/*
							if( EventIndex >= 300 )
								 EventIndex = 0;
							++EventIndex;
							swprintf( ch, L"\\BaseNamedObjects\\SysEvent%d", EventIndex );
							RtlInitUnicodeString( &EventName, ch );
							
							DbgPrint("%wZ\n", &EventName );
							
							pKEvent = IoCreateSynchronizationEvent( &EventName, &hThread );
							KeClearEvent( pKEvent );
							DealInfo.pKEvent = pKEvent;
							
							DealInfo.bSafe = TRUE;
							DealInfo.bDealing = FALSE;
							
							NdisAcquireSpinLock( &gSpinLock );
							DealInfos[count++] = DealInfo;
							NdisReleaseSpinLock( &gSpinLock );
									
              //��ʱ1000nm
							{
							LARGE_INTEGER LargeTime = {0};
							LargeTime.QuadPart = 1000;
							KeDelayExecutionThread( KernelMode, FALSE, &LargeTime );
						  }
						  
							KeSetEvent( pKEventForDll, 0, FALSE );
							DbgPrint("֪ͨӦ�ò����\n");
					 
						  DbgPrint("�����¼��ȴ�\n");
							KeWaitForSingleObject( pKEvent, Executive, KernelMode, 0, 0 );
							DbgPrint("�����е��¼���ȡ֪ͨ\n");
							 
							NdisAcquireSpinLock( &gSpinLock );
							for( index = 0; index < count; index++ )
							{
								DbgPrint("�������ȡ�����\n");
								if( strcmp( &(DealInfos[index].UrlInfo), &(DealInfo.UrlInfo) ) == 0 )
								{
									DealInfo.bSafe = DealInfos[index].bSafe;
									break;
								}
							}
							//ɾ���Ѿ����������URL
							
							if( index < count )
							{
								if( DealInfos[index].UrlInfo.pUrl == NULL )
									DbgPrint("1111");
								//	NdisAcquireSpinLock( &gSpinLock );
								//ExFreePool( DealInfos[index].UrlInfo.pUrl );
								//DealInfos[index].UrlInfo.pUrl = NULL;
								ExFreePool( DealInfo.UrlInfo.pUrl );
							
								for(; index < count; index++ )
								{
									DbgPrint("�����㴦��ȴ�Url����\n");
									DealInfos[index] = DealInfos[index+1];
								}
								count--;
								NdisReleaseSpinLock( &gSpinLock );
							}
							else
							{
								for( index = 0; index < count; index++ )
								{
									DbgPrint("�������ȡ�����\n");
									if( strcmp( &(DealInfos[index].UrlInfo), &(DealInfo.UrlInfo) ) == 0 )
									{
										DealInfo.bSafe = DealInfos[index].bSafe;
										break;
										}
								}
								DbgPrint("Not Found\n");
							}
							*/
							//�ٴλ��WaitingIndex
							//����ȫ�ְַ�����
							/*
							if( state != 0 )
							{
							 	WAITING_INFO WaitingSecInfo = {0};
					     	NdisAcquireSpinLock( &gWaitingSpinLock );
						  	 WaitingIndex = 0;
						  	 if( IsWaitingPacket(pTcpHeader->SeqNum, &WaitingIndex) )
					    	 {//���������Ӧ�óɹ�
					  	   	 WaitingSecInfo = WaitingInfo[WaitingIndex];
					  	   	 for(; WaitingIndex < WaitingCnt; WaitingIndex++ )
											 WaitingInfo[WaitingIndex] = WaitingInfo[WaitingIndex+1];
									 WaitingCnt--;
					  	  	 NdisReleaseSpinLock( &gWaitingSpinLock );
					  	    
					  	     ExFreePool( WaitingSecInfo.pHost );
					  	     ExFreePool( WaitingSecInfo.pUrl );
					  	     ExFreePool( WaitingSecInfo.pRefer );
					  	
					       }
					       else
					      	NdisReleaseSpinLock( &gWaitingSpinLock );
				   	  }
						
						  if( DealInfo.bSafe )
						  {//�����������������Ϊ��ȫ��ҳ������밲ȫ����ҳ����
							  
							  //�ȴ���ַ����������ַ�����������ַ�Ѵ��ڣ�����ʱ���ֱ�ӷŹ�
							  BOOLEAN bExist = FALSE;
								int MainRealUrlLen = 0;
								char* position = strchr( pRealUrl, '/' );
								if( position != NULL )
								  MainRealUrlLen = position - pRealUrl + 1;
								else
									MainRealUrlLen = len;
								bExist = ExistMainUrl( pRealUrl, MainRealUrlLen );
								if( bExist == TRUE )
									return STATUS_PASS;
								else
								{
									//������ַ���ڰ�ȫ�����У������ֵ
									SAFE_URL SafeUrl = {0};
									
									LARGE_INTEGER SysTime = {0};
		              LARGE_INTEGER LocTime = {0};
		              ULONG Seconds = 0;
		              KeQuerySystemTime( &SysTime );
	              	ExSystemTimeToLocalTime( &SysTime, &LocTime );
	              	RtlTimeToSecondsSince1980( &LocTime, &Seconds );
									
									SafeUrl.MainUrlLen = MainRealUrlLen;
									strncpy( SafeUrl.MainUrl, pRealUrl, MainRealUrlLen );
									SafeUrl.Seconds = Seconds;
									
									if( SafeUrlCnt < 500 )
									{//����δ����ֱ�����
										NdisAcquireSpinLock( &gSafeUrlSpinLock );
										SafeUrls[SafeUrlCnt++] = SafeUrl;
										NdisReleaseSpinLock( &gSafeUrlSpinLock );
									}
									else
									{//������������Ҫ�滻һ����ѡ��������ʵ��Ǹ�
										int index = 0;
										int oldest = 0;
										ULONG OldSeconds = 0;
										NdisAcquireSpinLock( &gSafeUrlSpinLock );
										OldSeconds = SafeUrls[index].Seconds;
										index++;
										for(; index < SafeUrlCnt; index++ )
										{
											if( SafeUrls[index].Seconds < OldSeconds )	
											{
												oldest = index;
												OldSeconds = SafeUrls[index].Seconds;
											}
										}
										SafeUrls[oldest] = SafeUrl;
										NdisReleaseSpinLock( &gSafeUrlSpinLock );
									}
									
								}
								return STATUS_PASS;
						  }
						  else
								return STATUS_DROP;
*/
							//NdisReleaseSpinLock( &gSpinLock );
							//DbgPrint("-------------------\n�˳�������\n");
							
							
							//ExFreePool( pUrlInfo );
							//pUrlInfo = NULL;
							
							/*
							if(pKEventForDll)
							{
								ObDereferenceObject(pKEventForDll); // delete event reference
								pKEventForDll = NULL;
							}	

							if(pKEventForSys)
							{
								ObDereferenceObject(pKEventForSys); // delete event reference
								pKEventForSys = NULL;
							}	
							*/
							/*
							if( bSafeUrl )
								return STATUS_PASS;
							else
								return STATUS_DROP; 
								*/
							
							
							//
							//return STATUS_DROP;
						}
						//����������ҳ�ְ�
					}
					//DbgPrint("Send TCP packet");
			   
			  }
				break;
			}
		}else if(pPacketContent[12] == 8 &&  pPacketContent[13] == 6 )
		{
			
			if(bRecOrSend)
				KdPrint(("Receive ARP packet"));
			else
				KdPrint(("Send ARP packet"));
			
		}else{
			
			if(bRecOrSend)
				KdPrint(("Receive unknown packet"));
			else
				KdPrint(("Send unknown packet"));
				
		}
                             

	 }
	 __finally
	{
		if( MmIsAddressValid(pPacketContentTmp) )
			NdisFreeMemory(pPacketContentTmp, 0, 0);
	}

	return STATUS_PASS;
}


FILTER_RESULT AnalysisNetBuffer( PNET_BUFFER pNetBuffer, BOOLEAN bRecOrSend )
{
	FILTER_RESULT status = STATUS_PASS; // Ĭ��ȫ��ͨ��
	DWORD dwBufferLen = 0;
	DWORD dwCopyLen = 0;
	PUCHAR pPacketContent = NULL;
	__try{
					dwBufferLen = NET_BUFFER_DATA_LENGTH(pNetBuffer);
					status = NdisAllocateMemoryWithTag( &pPacketContent, dwBufferLen, TAG); 
					if( status != NDIS_STATUS_SUCCESS )
					{
						status = NDIS_STATUS_FAILURE ;
						__leave;
					}
					dwCopyLen = dwBufferLen;
					NdisZeroMemory( pPacketContent, dwBufferLen );
					CopyBytesFromNetBuffer( pNetBuffer, &dwCopyLen, pPacketContent );
					if( dwCopyLen != dwBufferLen )
					{
						status = NDIS_STATUS_FAILURE ;
						__leave;
					}
							// ȡ�����ݰ����ݺ����潫�������ݽ��й��ˡ�
		// ��������������е�ʵ�֣������򵥵ش�ӡһЩ�ɶ���Log��Ϣ��
		if(pPacketContent[12] == 8 &&  pPacketContent[13] == 0 )  //is ip packet
		{	
			PIP_HEADER pIPHeader = (PIP_HEADER)(pPacketContent + IP_OFFSET);
			switch(pIPHeader->Protocol)
			{
			case PROT_ICMP:
				if(bRecOrSend)
					KdPrint(("Receive ICMP packet"));
				else
					KdPrint(("Send ICMP packet"));

				//
				// ȡ��ICMPͷ��������Ĺ����жϡ�
				// 
				break;
			case PROT_UDP:
				
				if(bRecOrSend)
					KdPrint(("Receive UDP packet"));
				else
					KdPrint(("Send UDP packet"));

				//
				// ȡ��UDPͷ��������Ĺ����жϡ�
				//
				
				break;
			case PROT_TCP:
				if(bRecOrSend)
				{
					KdPrint(("Receive TCP packet"));
					break;
					
				}
				else
				{//send packet
					
					PTCP_HEADER pTcpHeader = (PTCP_HEADER)( (UCHAR*)pIPHeader + ((pIPHeader->VIHL)&0xf)*4 );
					UINT HeaderLen = (((pTcpHeader->LenAndRes)&0x00f0)>>4)*4 + ((pIPHeader->VIHL)&0xf)*4 + 0xe;
				
					
					if( (dwBufferLen > HeaderLen) && (pTcpHeader->DestPort == 0x5000) )
					{//��Ӧ�ò����ݣ���Ӧ�ó̵Ķ˿ں�Ϊ80
						char *pAppData = pPacketContent + HeaderLen+12;
						UINT AppDataLen = dwBufferLen - HeaderLen-12;
						
						//��Ӧ�ò㽻���������
						DEAL_INFO DealInfo = {0};
						PKEVENT pKEvent = NULL;
						UNICODE_STRING EventName;
						WCHAR ch[200];
						HANDLE hThread;
						int index = 0;
						
						//�������ݰ�SeqNum���ж��Ƿ��Ǳ��ȴ���
						int WaitingIndex = 0;
						
						//�洢��ȡ�����ַ
						char pRealUrl[2048] = {0};
						int len = 0;
						
						
						//�洢��ȡ���refer�����ڼ�����
						char pRefer[100] = {0};
						int  ReferMainLen = 0;
						
						
						//�����ֽ����������ֽ���ת��
						pTcpHeader->SeqNum = Myhtonl( pTcpHeader->SeqNum );
						
						NdisAcquireSpinLock( &gWaitingSpinLock );
						if( IsWaitingPacket(pTcpHeader->SeqNum, &WaitingIndex) )
						{//�ǵȴ��ְ�
							
							int state = WaitingInfo[WaitingIndex].state;

							NdisReleaseSpinLock( &gWaitingSpinLock );
							//������GET����ֳ�3��/3�����ϰ������
							{
							
							char* pHost = NULL;
							int   begin = 0;
							int   end   = 0;
							switch( state )
							{
								case 1:
									{//δ��������
										UINT index = 0;
										begin = 0;
										for(; index < AppDataLen; index++ )
										{
											if( 0 == strncmp( pAppData+index, "\r\n", 2 ) )
											{//��Ϊδ����GET����3�����Ϸְ������Ա�Ȼ�ɹ�
												end = index;
											  break;
											}
										}
										break;
									}
								case 2:
									{//�������ּ���
										UINT index = 0;
										BOOLEAN bFound = FALSE;
										for(; index < AppDataLen; index++ )
										{
											if( !bFound )
											{
											   if( 0x20  == pAppData[index] )
											   {
												    begin = index+1;
												    bFound = TRUE;
												    continue;
										     }
										  }
											if( 0 == strncmp( pAppData+index, "\r\n", 2 ) )
											{
												end = index;
												break;
											}
										}
										break;
									}
								case 3:
									{//����ȫ������
										UINT index = 0;
										BOOLEAN bFound = TRUE;
										
										for(; index < AppDataLen; index++ )
										{
											if( !bFound )
										  {
											   if( 0 == strncmp( pAppData+index, "Host:", 5 ) )
											   {
											     	begin = index+6;
												    index = index+5;
												    continue;
											   }
										  }
										  if( 0 == strncmp( pAppData+index, "\r\n", 2 ) )
										  {
										  	end = index;
										  	break;
										  }	
										}
										break;
									}
									
							}
							//���ҵ�ʣ���Host��Ϣ����ʼλ�ã�δ���Ƿְ��� >= 3�������Ȼ����
							if( end > begin )
							{
								//�洢ʣ���Host��Ϣ
								WAITING_INFO WaitingFstInfo = {0};
								int SecHostLen = end - begin;
								pHost = (char*)ExAllocatePool( NonPagedPool, SecHostLen+1 );
								memset( pHost, 0, SecHostLen+1 );
								strncpy( pHost, pAppData+begin, SecHostLen );
								
								//����ȡ��ʣ��Host��Ϣ��ǰ����Host��Ϣ����
								NdisAcquireSpinLock( &gWaitingSpinLock );
								//���»�ȡWaitingIndex
								WaitingIndex = 0;
								if( IsWaitingPacket(pTcpHeader->SeqNum, &WaitingIndex) )
								{//��Ȼ���ҵ�
	                
									
									//�洢����Hostֵ����
									UINT HostLen = 0;
									UINT UrlLen = 0;
									UINT FstHostLen = 0;
									
									WaitingFstInfo = WaitingInfo[WaitingIndex];
									HostLen = WaitingFstInfo.HostLen + SecHostLen;
									UrlLen = WaitingFstInfo.UrlLen;
									FstHostLen = WaitingFstInfo.HostLen;
	                if( HostLen > 2048 )
	                {
	                	HostLen = 2048;
	                }
	                
	
	                WaitingInfo[WaitingIndex].HostLen = HostLen; 
									NdisReleaseSpinLock( &gWaitingSpinLock );
									
	                //����Hostֵ
	                strncat( WaitingFstInfo.pHost+FstHostLen, pHost, HostLen-FstHostLen );
	                ExFreePool( pHost );
	                pHost = NULL;
	                
	                
	                //�����Ϻ��Host����Ҫ��������������Լ������ļ�����ֱ�ӷŹ�
	                if( HostLen > strlen("reputation.cloudsvc.net") )
	                {
	                	if( 0 == strncmp( WaitingFstInfo.pHost, "reputation.cloudsvc.net", strlen("reputation.cloudsvc.net") ) )
	                	{//���������ֻ����������ǵļ�����Hostֵ�������
	                		ExFreePool( WaitingFstInfo.pHost );
					  	        ExFreePool( WaitingFstInfo.pUrl );
					  	        ExFreePool( WaitingFstInfo.pRefer );
	                		
							        return STATUS_PASS;
							      }
	                }
	                //������ַ���������pRealUrl
	                len = HostLen + WaitingFstInfo.UrlLen;
							  	if( len > 2048 )
							  	{//��ַ���ȳ���
							  		if( (len-UrlLen) < 2048 )//�����򳤶Ⱥϸ�ֻ�ͼ�������
							  		{
							  			strncpy( pRealUrl, WaitingFstInfo.pHost, len-UrlLen );
							  			len = len-UrlLen;
							  		}
							  		else//�����򳤶�Ҳ����,ȡHost�򣬵�һ����/��ǰ��ֵ
							  		{
							  			UINT index = 0;
							  			for(; index < len-UrlLen; index++ )
							  			{
							  				if( WaitingFstInfo.pHost[index] == '/' )
							  					break;
							  			}
							  			strncpy( pRealUrl, WaitingFstInfo.pHost, index+1 );
							  			len = index+1;
							  		}
							  	}
							  	else
							  	{//��ַ������
							  		strncpy( pRealUrl, WaitingFstInfo.pHost, len-UrlLen );
							  		strncat( pRealUrl, WaitingFstInfo.pUrl, UrlLen );
							  	}
							  	//��ȡRefer����ַ
							  	if( WaitingFstInfo.ReferLen > 0 )
							  	{
							  		//Refer:http://xxx.xxx,ǰ����7�����÷���
							  		UINT index = 7;
							  		for(; index < WaitingFstInfo.ReferLen; index++ )
							  		{
							  			if( WaitingFstInfo.pRefer[index] == '/' )
							  				break;
							  		}
							  		if( index < WaitingFstInfo.ReferLen )
							  			ReferMainLen = index-7;
							  		else
							  			ReferMainLen = WaitingFstInfo.ReferLen;
							  		if( ReferMainLen < 100 )
							  		  strncpy( pRefer, WaitingFstInfo.pRefer+7, ReferMainLen );
							  		else
							  			DbgPrint("ReferMainLen > 100\n");
							  	}
									goto Position;
									
						    }
						  }
							
						  }
						}
						else
						   NdisReleaseSpinLock( &gWaitingSpinLock );
						
						//���Ǳ��ȴ��ְ�,�ж��Ƿ�ΪGet�����
						if( IsHttpGetMethod(pAppData, AppDataLen ) )
						{//client������ҳ

							HTTP_INFO HttpUrl = {0};
							HTTP_INFO HttpVersion = {0};
							HTTP_INFO HttpReferer = {0};
							HTTP_INFO HttpHost = {0};
							

							
							UINT HostLen = 0;
							UINT UrlLen = 0;
							UINT ReferLen = 0;
							
							
							//��ʶ�÷ְ��ܷ���ȡ����ɵ�"����"+��ֵ,0:������1:"����"��������ֵ��������2��"����"��������3����
							UINT state = 0;
							
							//��ȡUrl��Ϣֵ
							HttpUrl = GetHttpGetMethodUrl( pAppData, AppDataLen );
						  //�жϸİ��Ƿ����HTTPЭ���ʽ
						  if( HttpUrl.end_offset < HttpUrl.begin_offset )
							{//Url�����򲻴���Url
								 return STATUS_DROP;
							}
							
							//��ȡHTTP�汾ֵ
							HttpVersion = GetHttpVersion( pAppData, HttpUrl, AppDataLen );
							if( HttpVersion.end_offset < HttpUrl.begin_offset )
								return STATUS_DROP;
							
							HttpReferer = GetHttpSubKey( pAppData, HttpVersion, "Referer:", AppDataLen, &state );
							HttpHost = GetHttpSubKey( pAppData, HttpVersion, "Host:", AppDataLen, &state );
							
							//��ȡ���������ַ
							if( HttpHost.end_offset == 0 )
								HostLen = 0;
							else
							{
							  HostLen = (HttpHost.end_offset - HttpHost.begin_offset+1);
							  HostLen = HostLen < 2048 ? HostLen: 2048;
							}
							if( HttpUrl.end_offset == 0 )
								UrlLen = 0;
							else
							{
							  UrlLen = HttpUrl.end_offset-HttpUrl.begin_offset+1;
							  UrlLen = UrlLen < 2048 ? UrlLen : 2048;
							}
							if( HttpReferer.end_offset == 0 )
								ReferLen = 0;
							else
							{
								ReferLen = HttpReferer.end_offset - HttpReferer.begin_offset+1;
								ReferLen = ReferLen < 2048 ? ReferLen : 2048;
							}
								
							//��������ǹ���ķְ���ֱ�ӷŹ�
							if( HostLen > 0 )
							{
								UINT StrLen = strlen("reputation.cloudsvc.net");
								UINT CmpLen = (StrLen > HostLen ? HostLen:StrLen);
								if( 0 == strncmp( pAppData+HttpHost.begin_offset, "reputation.cloudsvc.net", CmpLen ) )
							        return STATUS_PASS;
							  
							}
							
						
							//����ְ��в�����������Host��Ϣ
							if( state != 0 )
							{
								//��ʼ���÷ְ���Ϣ
								WAITING_INFO WaitingFstInfo = {0};
								
								//return STATUS_PASS;
								
								WaitingFstInfo.pHost = (char*)ExAllocatePool( NonPagedPool, 2048 );
								memset( WaitingFstInfo.pHost, 0, 2048 );
								if( HostLen > 0 )
								{
									memcpy( WaitingFstInfo.pHost, pAppData+HttpHost.begin_offset, HostLen );
									WaitingFstInfo.HostLen = HostLen;
								}
								
								WaitingFstInfo.pUrl = (char*)ExAllocatePool( NonPagedPool, 2048 );
								memset( WaitingFstInfo.pUrl, 0, 2048 );
								if( UrlLen > 0 )
								{
									memcpy( WaitingFstInfo.pUrl, pAppData+HttpUrl.begin_offset, UrlLen );
									WaitingFstInfo.UrlLen = UrlLen;
							  }
							  
							  WaitingFstInfo.pRefer = (char*)ExAllocatePool( NonPagedPool, 2048 );
							  memset( WaitingFstInfo.pRefer, 0, 2048 );
							  if( ReferLen > 0 )
							  {
							  	memcpy( WaitingFstInfo.pRefer, pAppData+HttpReferer.begin_offset, ReferLen );
							  	WaitingFstInfo.ReferLen = ReferLen;
							  }
							  
							  WaitingFstInfo.SeqNum = pTcpHeader->SeqNum + AppDataLen;
							  if( (pTcpHeader->LenAndRes)&0x0300 )
							  	WaitingFstInfo.SeqNum++;
							  WaitingFstInfo.state = state;

							  
							  //�洢�÷ְ���Ϣ
							  NdisAcquireSpinLock( &gWaitingSpinLock );
							  WaitingInfo[WaitingCnt++] = WaitingFstInfo;
							  NdisReleaseSpinLock( &gWaitingSpinLock );
							  
							  return STATUS_PASS;
							  
							}
							else//�ְ��а���������Host��Ϣ
							{		
						  	len = (HttpHost.end_offset-HttpHost.begin_offset+1)+(HttpUrl.end_offset-HttpUrl.begin_offset+1);
							  if( len > 2048 )
							  {
							  	if( HostLen < 2048 )//ȡ������
							  	{
							  		HTTP_INFO hp = {0};
							  		GetRealUrl( pRealUrl, HttpHost, hp, 2048, pAppData );
							  		len = HostLen;
							  	}
							  	else//ȡ�������е�һ��'\'ǰ��ֵ
							  	{
							  		UINT index = 0;
							  		for(; index < HostLen; index++ )
							  		{
							  			if( pAppData[HttpHost.begin_offset+index] == '/' )
							  				break;
							  		}
							  		strncpy( pRealUrl, pAppData+HttpHost.begin_offset, index+1 );
							  		len = index+1;
							  	}
								  
							  }
							  else
							     GetRealUrl( pRealUrl, HttpHost, HttpUrl, 2048, pAppData );
							  
							//��ȡRefer����ַ
							  if( ReferLen > 0 )
							  {
								   UINT index = HttpReferer.begin_offset+7;
								   for(; index-HttpReferer.begin_offset < ReferLen; index++ )
								   {
									    if( pAppData[index] == '/' )
										     break;
							     }
								   if( index-HttpReferer.begin_offset < ReferLen )
									    ReferMainLen = index - HttpReferer.begin_offset - 7;
								   else
									    ReferMainLen = ReferLen;
								   if( ReferMainLen < 100 )
								      strncpy( pRefer, pAppData+HttpReferer.begin_offset+7, ReferMainLen );
								   else
									    DbgPrint("ReferMainLen > 100\n");
						  	}
						  }
							
							//��Ҫ������ַ����pRealUrl,����Ϊlen
		Position:
							DbgPrint("Url��%s\n", pRealUrl );
							DbgPrint("Refer��%s\n", pRefer);
							//if( strstr( pRealUrl, "www.163.com") != NULL )
							if( dw_win7UrlCnt > 0 )
							{
								//��������ֹ��url�б�
								UINT r = 0;
								for( r = 0; r < dw_win7UrlCnt; r++ )
								{
									if( strstr( pRealUrl, psz_win7DangeUrl[r] ) != NULL )
										return STATUS_DROP;
								} 
							}
							else
							{
								//�۸����ݰ�
								
							}
							return STATUS_PASS;
						}
						//����������ҳ�ְ�
					}
					//DbgPrint("Send TCP packet");
			   
			  }
				break;
			}
		}else if(pPacketContent[12] == 8 &&  pPacketContent[13] == 6 )
		{
			
			if(bRecOrSend)
				DbgPrint("Receive ARP packet");
			else
				DbgPrint("Send ARP packet");
			
		}else{
			
			if(bRecOrSend)
				DbgPrint("Receive unknown packet");
			else
				DbgPrint("Send unknown packet");
				
		}
	}__finally
	{
		if(pPacketContent)
			NdisFreeMemory(pPacketContent, 0, 0);
	}
	return STATUS_PASS;
}



void CopyBytesFromNetBuffer( PNET_BUFFER NetBuffer, PDWORD cbDest, PVOID Dest )
{
	NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
	PMDL CurrentMdl = NET_BUFFER_CURRENT_MDL(NetBuffer);
	DWORD DestOffset = 0;
	while( ( DestOffset < *cbDest ) && CurrentMdl )
	{
		PUCHAR SrcMemory = MmGetSystemAddressForMdlSafe(CurrentMdl,LowPagePriority);
		ULONG Length = MmGetMdlByteCount(CurrentMdl);
		if( !SrcMemory )
		{
			Status = NDIS_STATUS_RESOURCES;
			break;
		}
		if( DestOffset == 0 )
		{
			ULONG MdlOffset = NET_BUFFER_CURRENT_MDL_OFFSET(NetBuffer);
			SrcMemory += MdlOffset;
			Length = MdlOffset;
		}
		Length = Length < (*cbDest-DestOffset)?Length:(*cbDest-DestOffset);
		NdisMoveMemory( (PUCHAR)Dest+DestOffset, SrcMemory, Length );
		DestOffset += Length;
		
		CurrentMdl = NDIS_MDL_LINKAGE(CurrentMdl);
	}
	
	if( Status == NDIS_STATUS_SUCCESS )
		*cbDest = DestOffset;
	return;
}

BOOLEAN IsHttpGetMethod( char* pAppData, int AppDataLen  )
{
	if( AppDataLen < 3 )
		return FALSE;
	if( (*pAppData == 'G') && (*(pAppData+1) == 'E') && (*(pAppData+2) == 'T') )
		return TRUE;

	return FALSE;
}

HTTP_INFO GetHttpGetMethodUrl( char* pAppData, int AppDataLen )
{
	HTTP_INFO HttpUrl = {0};
	int len = 0;
	
	HttpUrl.begin_offset = 4;
	pAppData += HttpUrl.begin_offset;
	while( *pAppData != 0x20 )
	{
		len++;
		pAppData++;
		if( (HttpUrl.begin_offset+len) > AppDataLen )
		{//����������������������Url�Ѿ�����������󳤶�,��������
			len = 0;
			break;
		}
	}
	HttpUrl.end_offset = HttpUrl.begin_offset + len -1;
	return HttpUrl;
}

HTTP_INFO GetHttpVersion( char* pAppData, HTTP_INFO HttpUrl, int AppDataLen )
{
	HTTP_INFO HttpVersion = {0};
	int len = 0;

	HttpVersion.begin_offset = HttpUrl.end_offset + 0x2;
	while( pAppData[HttpVersion.begin_offset] != 0xd || pAppData[HttpVersion.begin_offset+1] != 0xa )
	{
		len++;
		pAppData++;
		if( (HttpVersion.begin_offset+len) > AppDataLen )
		{//����������������������Url�Ѿ�����������󳤶�,��������
			len = 0;
			break;
		}
	}
	
	HttpVersion.end_offset = HttpVersion.begin_offset + len -1;

	return HttpVersion;
	
}

HTTP_INFO GetHttpSubKey( char* pAppData, HTTP_INFO HttpVersion, char* pSubKey, int AppDataLen, int* state )
{
	HTTP_INFO HttpInfo = {0};
	int begin = HttpVersion.end_offset + 0x3;
	int len = 0;

	
	if( IsFindSubKey(pAppData, &begin, pSubKey, AppDataLen) )
	{//������������
		int SubKeyLen = strlen(pSubKey);
		int StartPosition = begin + SubKeyLen + 1;
		int EndPosition = StartPosition;
		BOOLEAN bOver = FALSE;
		
		while( EndPosition < AppDataLen )
		{
			if( 0 == strncmp( pAppData+EndPosition, "\r", 1 ) )
			{
				bOver = TRUE;
				break;
			}
			EndPosition++;
		}
		if( bOver == TRUE )
		{//�ҵ������ļ�ֵ,�����������1.����"\n\r";2.����"\n"
			*state = 0;
		}
		else if( EndPosition != StartPosition )
		{//��������ֵ,���ð����������ּ�ֵ
			*state = 1;
		}
		else
		{//���ָð��Ƿ����"Host:"��Ŀո�,��ʱ�ð��Բ����κμ�ֵ
			if( StartPosition-1 == AppDataLen )//�ո��ڸð�
				*state = 2;
			else//�ո��ڸð���
				*state = 1;
			return HttpInfo;		
		}
		
		HttpInfo.begin_offset = StartPosition;
	  HttpInfo.end_offset = EndPosition-1;
	  return HttpInfo;
	}
	else //���������,1.��������2.��������
	{
		UINT StartPosition = AppDataLen - 1;
		BOOLEAN bFound = FALSE;
		if( 0 == strncmp( pAppData+AppDataLen-0x4, "\r\n\r\n", 4 ) )
		{
			*state = 0;
			return HttpInfo;
		}
		//δ������������������������硰Host:��,1.��ȫ��������2.��������
    if( pAppData[StartPosition] == '\r' || pAppData[StartPosition] == '\n' )
    {
    	*state = 3;
    	return HttpInfo;
    }
		for(; StartPosition > 0; StartPosition-- )
		{
			if( pAppData[StartPosition-1] == '\r' && pAppData[StartPosition] == '\n' )
			{
				bFound = TRUE;
				break;
			}
		}
		if( bFound )
		{//��Ȼ���ҵ���������һ��"\n\r"
			if( (UINT)((AppDataLen - StartPosition)) < strlen(pSubKey) && 0 == strncmp( pSubKey, pAppData+StartPosition+1, AppDataLen - StartPosition ) )
				*state = 2;
			else
				*state = 3;
		}
		return HttpInfo;
	}
	
	return HttpInfo;
}

BOOLEAN IsFindSubKey( char* pAppData, int* begin, char* pSubKey, int AppDataLen )
{
	BOOLEAN bFind = FALSE;
	int SubKeyLen = strlen(pSubKey);
	int TempPosition = *begin;
	while( TempPosition < AppDataLen-SubKeyLen )
	{
		int index = 0;
		for(; index < SubKeyLen; index++ )
		{
			if( pAppData[TempPosition+index] != pSubKey[index] )
				break;
		}

		if( index == SubKeyLen )
		{
			bFind = TRUE;
			*begin = TempPosition;
			break;
		}
		else
		{
			TempPosition++;
		}
	}
	return bFind;

}

void GetRealUrl( char* pRealUrl, HTTP_INFO pHost, HTTP_INFO pUrl, int len, char* pAppData )
{
	int index = 0;
	

	   while( pHost.begin_offset < pHost.end_offset+1 )
	   {
		    pRealUrl[index++] = pAppData[pHost.begin_offset++];
		    if( index > len-1 )
			  break;
	   }
 

 
  
	   while( pUrl.begin_offset < pUrl.end_offset+1 && pUrl.begin_offset)
	   {
		    pRealUrl[index++] = pAppData[pUrl.begin_offset++];
		    if( index > len-1 )
		  	break;
	   }
  

}


BOOLEAN IsWaitingPacket( ULONG SeqNum, int* WaitingIndex )
{
	if( WaitingCnt == 0 )
		return FALSE;
	else
	{
		int index = 0;
		for( ; index < WaitingCnt; index++ )
		{
			if( WaitingInfo[index].SeqNum == SeqNum )
			{
				*WaitingIndex = index;
				return TRUE;
			}
		}
		return FALSE;
		
	}
	
}

ULONG Myhtonl( ULONG hSeqNum )
{
	int index = 0;
	ULONG nSeqNum = 0;
	int size = sizeof( hSeqNum );
	char* pNSeqNum = (char*)&nSeqNum;
	char* pHSeqNum = (char*)&hSeqNum;
	for(; index < size; index++ )
	{
		*(pNSeqNum+size-1-index) = *(pHSeqNum+index);
	}
	return nSeqNum;
}

/*
���ܣ��ж�pMainUrl��ָ�������ַ���Ƿ����Ѽ����İ�ȫ��ַ��������������ʱ��ֵ������true������ֱ�ӷ���false
*/
/*
BOOLEAN ExistMainUrl( char* pMainUrl, int MainUrlLen )
{
	int index = 0;
	BOOLEAN bExist = FALSE;
	SAFE_URL SafeUrl = {0};
	
	ASSERT( MainUrlLen < 100 );
	ASSERT( pMainUrl != NULL );
	
	NdisAcquireSpinLock( &gSafeUrlSpinLock );
	for(; index < SafeUrlCnt; index++ )
	{
		if( 0 == strncmp( SafeUrls[index].MainUrl, pMainUrl, MainUrlLen ) )
		{
			SafeUrl = SafeUrls[index];
			bExist = TRUE;
			break;
		}
	}
	NdisReleaseSpinLock( &gSafeUrlSpinLock );
	if( bExist )
	{
		LARGE_INTEGER SysTime = {0};
		LARGE_INTEGER LocTime = {0};
		ULONG Seconds = 0;
		KeQuerySystemTime( &SysTime );
		ExSystemTimeToLocalTime( &SysTime, &LocTime );
		RtlTimeToSecondsSince1980( &LocTime, &Seconds );
		if( Seconds - SafeUrl.Seconds > 28800 )//8*60*60 == 28800
		{//����8Сʱ��ֱ�ӷ���false
			return FALSE;		
		}
		else
		{//С��8Сʱ����ȫ��ҳ������������ʱ��
			index = 0;
			NdisAcquireSpinLock( &gSafeUrlSpinLock );
			for(; index < SafeUrlCnt; index++ )
			{
				if( 0 == strncmp( SafeUrls[index].MainUrl, pMainUrl, MainUrlLen ) )
				{
					if( SafeUrls[index].Seconds < Seconds )
						SafeUrls[index].Seconds = Seconds;
					break;
				}
			}
			NdisReleaseSpinLock( &gSafeUrlSpinLock );
			return TRUE;
		}
		
	}
	else
		return FALSE;
	
}
*/

FILTER_RESULT NeedDealPacket( PNDIS_PACKET Packet )
{
	      //�洢���ݰ�����
        PUCHAR pPacketContent = NULL;
        FILTER_RESULT Status = STATUS_PASS;
        
        //�������ݰ�ʱ�������
        UINT PhysicalBufferCount = 0;//�ڴ��е��������
        UINT BufferCount = 0;//�����ݰ��е�NDIS_BUFFER����
        PNDIS_BUFFER pNdisBuffer = NULL;//ָ��һ��NDIS_BUFFER��
        UINT TotalPacketLength = 0; //���ݰ��е��ܳ���
        UINT DataOffset = 0;
        
		    KIRQL kIrql;
		    kIrql = KeGetCurrentIrql();
		     Status = NdisAllocateMemoryWithTag( &pPacketContent, 2048, TAG );
        if( Status != NDIS_STATUS_SUCCESS )
        {
        	Status = NDIS_STATUS_FAILURE;
        	return Status;
        }
        NdisZeroMemory( pPacketContent, 2048 );
        
        //�������ݰ������Ȼ�ȡ��һ��NDIS_BUFFER����Ȼ������NdisGetNextBufferö�����еİ�
        NdisQueryPacket( Packet, &PhysicalBufferCount, &BufferCount, &pNdisBuffer, &TotalPacketLength );
        while( TRUE )
        {
        	//��ȡNdis_Buffer�����⻺��ĵ�ַ
        	PUCHAR tempBuff = NULL;
        	UINT copysize = 0;
        	NdisQueryBufferSafe( pNdisBuffer, &tempBuff, &copysize, NormalPagePriority );
        	if( tempBuff != NULL )
        	{
        		NdisMoveMemory( pPacketContent + DataOffset, tempBuff, copysize );
        		DataOffset += copysize;
        	}
        	//��ȡ��һ��Ndis_Buffer���ݰ�
        	NdisGetNextBuffer( pNdisBuffer, &pNdisBuffer );
          if( pNdisBuffer == NULL )
          	break;
        }
        //���ݰ��������Ѿ�������pPacketContent,����ΪTotalPacketLength
        
        //�ж��Ƿ�ΪTCP������Ҫ��˿�Ϊ80
        if( pPacketContent[12] == 8 && pPacketContent[13] == 0 )//�ж��Ƿ���IP��
        {
        	PIP_HEADER pIPHeader = (PIP_HEADER)( pPacketContent + IP_OFFSET );
        	if( pIPHeader->Protocol == PROT_TCP )//�ж��Ƿ���TCP
        	{
        		//�ж϶˿��Ƿ�Ϊ80�������ް�����
        		//TCP��ͷ
        		PTCP_HEADER pTcpHeader = (PTCP_HEADER)( (UCHAR*)pIPHeader + ((pIPHeader->VIHL)&0xf)*4 );
        		//��ͷ����
					  int HeaderLen = (((pTcpHeader->LenAndRes)&0x00f0)>>4)*4 + ((pIPHeader->VIHL)&0xf)*4 + 0xe;
        		if( (DataOffset > (UINT)HeaderLen) && (pTcpHeader->DestPort == 0x5000) )
        		{//��Ӧ�ò����ݣ���Ӧ�ó̵Ķ˿ں�Ϊ80
        			char *pAppData = pPacketContent + HeaderLen;
						  int AppDataLen = DataOffset - HeaderLen;
						  
						  //���ȸ���SeqNum�ж��Ƿ񱻵ȴ��ְ�
						  int WaitingIndex = 0;
						  //����SeqNum���������ֽ���������ֽ���ת��
						  pTcpHeader->SeqNum = Myhtonl( pTcpHeader->SeqNum );
						  //����Ҫ����ȫ�����飬��������������ͬ��
						  NdisAcquireSpinLock( &gWaitingSpinLock );
					  	if( IsWaitingPacket(pTcpHeader->SeqNum, &WaitingIndex) )
					  	{//�ǵȴ��ְ�
              
                //���IRQL == DISPACH_LEVEL��ֱ�Ӷ���
		            if( kIrql == DISPATCH_LEVEL )
		            {
		          	NdisReleaseSpinLock( &gWaitingSpinLock );
		          	NdisFreeMemory( pPacketContent, 0 , 0 );
		          	return STATUS_DROP;
		            }
		            else
		            {
							   NdisReleaseSpinLock( &gWaitingSpinLock );
							   NdisFreeMemory( pPacketContent, 0 , 0 );
							   return STATUS_NEEDDEAL;
					   	  }
					  	}
						  else
						  {//���Ǳ��ȴ���
						     NdisReleaseSpinLock( &gWaitingSpinLock );
						     //�ж��Ƿ���Get��,�����ǣ�ֱ�ӷŹ�
						     if( IsHttpGetMethod(pAppData, AppDataLen ) )
						     {//client������ҳ
							

							    //���IRQL == DISPACH_LEVEL,ֱ�Ӷ���
							    if( kIrql == DISPATCH_LEVEL )
							    {
							    	NdisFreeMemory( pPacketContent, 0, 0 );
							      return STATUS_DROP;
							    }
							    else
							    {
							    	NdisFreeMemory( pPacketContent, 0, 0 );
							    	return STATUS_NEEDDEAL;
							    }
								   			   
						     }
						     else
						     {
						      	NdisFreeMemory( pPacketContent, 0, 0 );
						    	  return STATUS_PASS;
						     }
        		  }
        		}
        		else
        		{//�˿ڲ�Ϊ80�������ް����ݣ�ֱ�ӷŹ�
        			NdisFreeMemory( pPacketContent, 0, 0 );
        			return STATUS_PASS;
        		}
        	}
        	else
        	{//��IP��������TCP����ֱ�ӷŹ�
        		NdisFreeMemory( pPacketContent, 0, 0 );
        		return STATUS_PASS;
        	}
        }
        else
        {//��IP����ֱ�ӷŹ�
        	NdisFreeMemory(pPacketContent, 0, 0);
        	return STATUS_PASS;
        }
        
}