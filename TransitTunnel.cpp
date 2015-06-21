#include <string.h>
#include "I2PEndian.h"
#include "Log.h"
#include "RouterContext.h"
#include "I2NPProtocol.h"
#include "Tunnel.h"
#include "Transports.h"
#include "TransitTunnel.h"

namespace i2p
{
namespace tunnel
{	
	TransitTunnel::TransitTunnel (uint32_t receiveTunnelID, 
	    const uint8_t * nextIdent, uint32_t nextTunnelID, 
		const uint8_t * layerKey,const uint8_t * ivKey): 
			m_TunnelID (receiveTunnelID),  m_NextTunnelID (nextTunnelID), 
			m_NextIdent (nextIdent)
	{	
		m_Encryption.SetKeys (layerKey, ivKey);
	}	

	void TransitTunnel::EncryptTunnelMsg (std::shared_ptr<I2NPMessage> tunnelMsg)
	{		
		m_Encryption.Encrypt (tunnelMsg->GetPayload () + 4, tunnelMsg->GetPayload () + 4); 
	}	

	TransitTunnelParticipant::~TransitTunnelParticipant ()
	{
	}	
		
	void TransitTunnelParticipant::HandleTunnelDataMsg (std::shared_ptr<i2p::I2NPMessage> tunnelMsg)
	{
		EncryptTunnelMsg (tunnelMsg);
		
		m_NumTransmittedBytes += tunnelMsg->GetLength ();
		htobe32buf (tunnelMsg->GetPayload (), GetNextTunnelID ());
		FillI2NPMessageHeader (tunnelMsg.get (), eI2NPTunnelData); // TODO
		m_TunnelDataMsgs.push_back (tunnelMsg);
	}

	void TransitTunnelParticipant::FlushTunnelDataMsgs ()
	{
		if (!m_TunnelDataMsgs.empty ())
		{	
			auto num = m_TunnelDataMsgs.size ();
			if (num > 1)
				LogPrint (eLogDebug, "TransitTunnel: ",GetTunnelID (),"->", GetNextTunnelID (), " ", num);
			i2p::transport::transports.SendMessages (GetNextIdentHash (), m_TunnelDataMsgs);
			m_TunnelDataMsgs.clear ();
		}	
	}	
		
	void TransitTunnel::SendTunnelDataMsg (std::shared_ptr<i2p::I2NPMessage> msg)
	{	
		LogPrint (eLogError, "We are not a gateway for transit tunnel ", m_TunnelID);
	}		

	void TransitTunnel::HandleTunnelDataMsg (std::shared_ptr<i2p::I2NPMessage> tunnelMsg)
	{
		LogPrint (eLogError, "Incoming tunnel message is not supported  ", m_TunnelID);
	}	
		
	void TransitTunnelGateway::SendTunnelDataMsg (std::shared_ptr<i2p::I2NPMessage> msg)
	{
		TunnelMessageBlock block;
		block.deliveryType = eDeliveryTypeLocal;
		block.data = msg;
		std::unique_lock<std::mutex> l(m_SendMutex);
		m_Gateway.PutTunnelDataMsg (block);
	}		

	void TransitTunnelGateway::FlushTunnelDataMsgs ()
	{
		std::unique_lock<std::mutex> l(m_SendMutex);
		m_Gateway.SendBuffer ();
	}	
		
	void TransitTunnelEndpoint::HandleTunnelDataMsg (std::shared_ptr<i2p::I2NPMessage> tunnelMsg)
	{
		EncryptTunnelMsg (tunnelMsg);
		
		LogPrint (eLogDebug, "TransitTunnel endpoint for ", GetTunnelID ());
		m_Endpoint.HandleDecryptedTunnelDataMsg (tunnelMsg); 
	}
		
	TransitTunnel * CreateTransitTunnel (uint32_t receiveTunnelID,
		const uint8_t * nextIdent, uint32_t nextTunnelID, 
	    const uint8_t * layerKey,const uint8_t * ivKey, 
		bool isGateway, bool isEndpoint)
	{
		if (isEndpoint)
		{	
			LogPrint (eLogInfo, "TransitTunnel endpoint: ", receiveTunnelID, " created");
			return new TransitTunnelEndpoint (receiveTunnelID, nextIdent, nextTunnelID, layerKey, ivKey);
		}	
		else if (isGateway)
		{	
			LogPrint (eLogInfo, "TransitTunnel gateway: ", receiveTunnelID, " created");
			return new TransitTunnelGateway (receiveTunnelID, nextIdent, nextTunnelID, layerKey, ivKey);
		}	
		else	
		{	
			LogPrint (eLogInfo, "TransitTunnel: ", receiveTunnelID, "->", nextTunnelID, " created");
			return new TransitTunnelParticipant (receiveTunnelID, nextIdent, nextTunnelID, layerKey, ivKey);
		}	
	}		
}
}
