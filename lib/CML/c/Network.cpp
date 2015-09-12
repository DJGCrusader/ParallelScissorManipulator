/********************************************************/
/*                                                      */
/*  Copley Motion Libraries                             */
/*                                                      */
/*  Copyright (c) 2002 Copley Controls Corp.            */
/*                     http://www.copleycontrols.com    */
/*                                                      */
/********************************************************/

/** \file

This file holds code for the top level CANopen class.
This class is used for over all control of the CANopen network.

*/

#include "CML_Settings.h"
#include "CML_Error.h"
#include "CML_Network.h"
#include "CML_Node.h"

CML_NAMESPACE_USE();

// static Network error objects
CML_NEW_ERROR( NetworkError, NodeIdUsed,        "A node with the specified ID is already present on the network" );

/***************************************************************************/
/**
Return a pointer to the network information union embedded in this node.
This union contains data related to the node that is owned by the network
layer object.

@param n Pointer to the node object
@return Pointer to the node information
*/
/***************************************************************************/
NetworkNodeInfo *Network::GetNodeInfo( Node *n )
{
   return n->nodeInfo;
}

/***************************************************************************/
/**
Set the network's node information for this node.  This function is used
internally by the network classes.

@param n Pointer to the node to update
@param ni Pointer to the node information
*/
/***************************************************************************/
void Network::SetNodeInfo( Node *n, NetworkNodeInfo *ni )
{
   n->nodeInfo = ni;
}

/// Return the maximum number of bytes that can be sent in an SDO message.
/// For CANopen this is always 8 (the max size of a CAN frame).  For EtherCAT
/// it's the size of the mailbox buffer, and is node specific
/// @param n The node to query
/// @return The maximum number of bytes in an SDO transmit message
int32 Network::maxSdoToNode( Node *n )
{
   return 8;
}

/// Return the maximum number of bytes that can be received in an SDO message.
/// For CANopen this is always 8 (the max size of a CAN frame).  For EtherCAT
/// it's the size of the mailbox buffer, and is node specific
/// @param n The node to query
/// @return The maximum number of bytes in an SDO receive message
int32 Network::maxSdoFromNode( Node *n )
{
   return 8;
}

