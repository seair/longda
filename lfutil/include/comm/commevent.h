// __CR__
// Copyright (c) 2008-2010 Longda Corporation
// All Rights Reserved
// 
// This software contains the intellectual property of Longda Corporation
// or is licensed to Longda Corporation from third parties.  Use of this 
// software and the intellectual property contained therein is expressly
// limited to the terms and conditions of the License Agreement under which 
// it is provided by or on behalf of Longda.
// __CR__


#ifndef __COMM_EVENT_H__
#define __COMM_EVENT_H__
#define ID_STR(ID,STR)          \
            case(ID):           \
                return(STR)
                
#include <string>
#include <vector>

#include "seda/stageevent.h"
#include "net/iovec.h"
#include "net/endpoint.h"

#include "comm/message.h"

/**
 *  @author Longda
 *  @date   5/18/07
 */

class SedaStats;

//! Message descriptor type
/**
 * MsgDesc is used for initializing CommEvent objects and extracting 
 * the information associated with the request and response messages.
 */
struct MsgDesc {
    MsgDesc(): message(NULL), attachFileOffset(0), attachFileLen(0) {}
    MsgDesc(Message* msg): message(msg), attachFileOffset(0), attachFileLen(0) {}

    MsgDesc& operator= (const MsgDesc &msgDesc)
    {
        message          = msgDesc.message;
        attachMems       = msgDesc.attachMems;
        attachFileOffset = msgDesc.attachFileOffset;
        attachFileLen    = msgDesc.attachFileLen;
        attachFilePath   = msgDesc.attachFilePath;

        return *this;
    }

    void cleanupAttachMem()
    {
        for (std::vector<IoVec::vec_t*>::iterator it = attachMems.begin();
                it != attachMems.end(); it++)
        {
            delete (char *)(*it)->base;

            delete (*it);
        }

        attachMems.clear();
    }

    void cleanup()
    {
        if (message)
        {
            delete message;
        }

        cleanupAttachMem();

        cleanFile();

    }

    void cleanFile()
    {
        attachFileOffset = 0;
        attachFileLen = 0;
        attachFilePath.clear();
    }

    /**
     * just cleanup container, don't delete memory;
     */
    void cleanupContainer()
    {
        attachMems.clear();

        cleanFile();

    }




    Message*                      message;           //!< message object
    std::vector<IoVec::vec_t*>    attachMems;        //!< attach memories
    u64_t                         attachFileOffset;  //!< attach file offset
    u64_t                         attachFileLen;     //!< attach file send count
    std::string                   attachFilePath;    //!< attach file path
};



class CommEvent : public StageEvent
{
public:
    //! CommEvent completion status type
    /**
     * Indicates the local completion status of CommEvent. This should not
     * be confused with a server request response with an unsucessful
     * message status. In the latter case, CommEvent's local status will
     * be SUCCESS indicating that the RPC communication related to the
     * protocol messages has been completed successfully.
     */
    typedef enum
    {
        SUCCESS = 0, //!< successful local completion
        INVALID_PARAMETER, //!< invalid parameter
        PENDING, //!< event is being processed
        CONN_FAILURE, //!< CommStage failed to connect to peer
        MALFORMED_MESSAGE, //!< message contained in event is malformed
        VERSION_MISMATCH, //!< version not match
        SEND_FAILURE, //!< CommStage reported transmission failure
        BUFFER_OVERFLOW, //!< server has replied with more data than posted
        STAGE_CLEANUP, //!< CommStage is in a cleanup mode
        RESOURCE_FAILURE, //!< local resource failure
        BAD_TIMEOUT_VAL, //!< unacceptable timeout value
        TIMEDOUT
    } //!< event has timedout
    status_t;

    //! Convert the status id to string
    /**
     *  @brief To help report the status to a descriptive string
     *  @param[in]  status
     *  @return     The description as a string
     */
    static std::string statusStr(int status)
    {
        switch (status)
        {
        ID_STR( SUCCESS, "Successful local completion");
        ID_STR( INVALID_PARAMETER, "Invalid parameters");
        ID_STR( PENDING, "Event is being processed");
        ID_STR( CONN_FAILURE, "CommStage failed to connect to peer");
        ID_STR( MALFORMED_MESSAGE, "Message contained in event is malformed");
        ID_STR( SEND_FAILURE, "CommStage reported transmission failure");
        ID_STR( BUFFER_OVERFLOW,
                "Server has replied with more data than posted");
        ID_STR( STAGE_CLEANUP, "CommStage is in a cleanup mode");
        ID_STR( RESOURCE_FAILURE, "Local resource failure");
        ID_STR( BAD_TIMEOUT_VAL, "Unacceptable timeout value");
        ID_STR( TIMEDOUT, "Event has timedout");

        default:
        {
            return "Unknown";
        }
        }
    }

    //! Default constructor
    /**
     * @post event is contructed and both request and response set to 0
     */
    CommEvent();
    //! Constructor
    /**
     * @param[in]   req     pointer to a request message desriptor
     * @param[in]   resp    pointer to a response message desriptor (optional)
     * 
     * @post event is contructed with a request message descriptor and
     * @post optionally a response message descriptor
     */
    CommEvent(MsgDesc* req, MsgDesc* resp = 0);
    //! Constructor
    /**
     * @param[in]   reqMsg  pointer to a request message 
     * @param[in]   respMsg pointer to a response message (optional)
     * 
     * @post event is contructed with a request message and with an
     *       optional response message
     */
    CommEvent(Message* reqMsg, Message* respMsg = 0);
    //! Destructor
    /**
     * @pre  all processing related to the event is completed
     * @post request and response messages and attachment vectors are freed
     */
    ~CommEvent();
    //! Initialize the request message descriptor
    /**
     * @param[in]   req     request message descriptor
     */
    void setRequest(MsgDesc* req);
    //! Set the request message
    /**
     * @param[in]   reqMsg  request message
     */
    void setRequestMsg(Message* reqMsg);
    //! Initialize the response message descriptor
    /**
     * @param[in]   resp    response message descriptor
     */
    void setResponse(MsgDesc* resp);
    //! Set the response message
    /**
     * @param[in]   respMsg response message
     */
    void setResponseMsg(Message* respMsg);
    //! Initialize the request message descriptor
    /**
     * @return  the value of the request message descriptor
     */
    MsgDesc& getRequest();
    //! Initialize the request message descriptor
    /**
     * @return  the pointer to the request message
     */
    Message* getRequestMsg();
    //! Initialize the request message descriptor
    /**
     * @return  the value of the response message descriptor
     */
    MsgDesc& getResponse();
    //! Initialize the request message descriptor
    /**
     * @return  the pointer to the response message
     */
    Message* getResponseMsg();
    //! Set request ID
    /**
     * @param[in] reqId the request ID to be set
     *
     * @post    the member rquestId is set to the value of reqId
     */
    void setRequestId(unsigned int reqId);
    //! Get request ID
    /**
     * Get the value of the requestId member set by setRequestId
     *
     * @return  value of the requestId member
     */
    unsigned int getRequestId();
    //! Transfer ownership of the request message descriptor
    /**
     * Similar in functionality to getRequest, except that also detaches
     * the request message resources from CommEvent. Responsibility for
     * deallocating all resources is shifted to caller. Caller can use
     * cleanupMsgDesc method to cleanup the resources.
     *
     * @return  request message descriptor
     *
     * @post    request MsgDesc is detached from event. Caller responsible
     * for cleanup. Caller can use cleanupMsgDesc for resource deallocation.
     */
    MsgDesc adoptRequest();
    //! Transfer ownership of the response message descriptor
    /**
     * Similar to functionality to getResponse, except that also detaches
     * the response message resources from CommEvent . Responsibility for
     * deallocating all resources is shifted to caller. Caller can use
     * cleanupMsgDesc method to cleanup the resources.
     *
     * @return  response message descriptor
     *
     * @post    response MsgDesc is detached from event. Caller responsible
     * for cleanup. Caller can use cleanupMsgDesc for resource deallocation.
     */
    MsgDesc adoptResponse();
    //! Set local completion status
    /**
     * Used for indicating failure in the local event processing. Callbacks
     * registered for this event will be informed that the event has
     * failed.
     *
     * @param[in]   stat    the event's status
     *
     * @post    event is marked with the completion status
     */
    void setStatus(CommEvent::status_t stat);
    //! Get local completion status
    /**
     * Callbacks registered for this event can check if the event has
     * succeeded or failed by inspecting the event's completion status.
     * This method checks only the local completion status, not the content
     * of the response message attached to the event object.
     *
     * @return  event's local completion status
     */
    CommEvent::status_t getStatus();
    //! Check if the event has failed
    /**
     * Check whether the event's status has been set to a value other 
     * than SUCCESS in the processing pipeline.
     *
     * @return true if the event has failed, false otherwise
     */
    bool isfailed();
    //! Sends an error response
    /**
     * This method is semantically equivalent to StageEvent->done(). It must
     * be the last method executed on the event object as it internally
     * calls done(), which unrolls the callback stack and deletes the event.
     * The method assumes that the event has a correct request message, which
     * is used to extract the protocol, the source, and the target information. 
     * 
     * @param[in]    errcode     error code for the response
     * @param[in]    errmesg     error message for the response
     * @param[in]    exception   boolean indicating if the response is exception
     *
     * @return  true if the response was successfuly created and event completed
     * 
     * @pre     correct request message in event
     * @post    a response message set and event completed
     */
    bool doneWithErrorResponse(CommEvent::status_t eventErrCode, int rspErrCode,
            const char* errmsg);
    /**
     * just set the status and done
     */
    void completeEvent(CommEvent::status_t stev);
    //! Deallocates the resources associated with MsgDesc
    /**
     * Frees the array of pointers to IoVec::vec_t structures, the 
     * structures themselves and the buffer associated with each vec_t 
     * structure. This method is invoked by ~CommEvent on the request 
     * response MsgDesc before the event is destoryed.
     *
     * @param[in]   md  message descriptor to be cleaned
     * 
     * @pre     the resources associated with md are not in use
     * @post    all resources are deallocated
     */
    static void cleanupMsgDesc(MsgDesc& md);
    //! Indicates server side created event
    /**
     * Completion of events depends on whether they are created (generated)
     * by the client or server side of the RPC protocol. This metod is used
     * by the server side of the CommStage class to indicate that the event
     * should be completed when the last IoVec from the response is sent.
     *
     * @post     event is marked as generated by the server side
     */
    void setServerGen();
    //! Check if event is server-side created
    /**
     * Checks if the event has been created by the server side of CommStage.
     *
     * @return  true if event is created at the server side
     */
    bool isServerGen();

    int getSock() const
    {
        return sock;
    }

    void setSock(int sock)
    {
        this->sock = sock;
    }

    //! Get the target host
    EndPoint& getTargetEp()
    {
        return targetEp;
    }

private:
    MsgDesc  request;      //!< descriptor of request message
    MsgDesc  response;     //!< descriptor of response message
    status_t status;       //!< completion status of event
    u32_t    requestId;    //!< at server carries the incoming request id
    bool     serverGen;    //!< is event creted by the server side
    int      sock;         //!< socket, setted when serverGen set
    EndPoint targetEp;     //!< target endpoint


private:
    //! Copy constructor
    /**
     * Declared as private to avoid accidental invocation of the default one
     */
    CommEvent(const CommEvent& cev);
    //! Assignment operator
    /**
     * Declared as private to avoid accidental invocation of the default one
     */
    CommEvent& operator =(const CommEvent& cev);
};

#endif // __COMM_EVENT_H__
