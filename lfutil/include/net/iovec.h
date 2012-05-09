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


#ifndef _IOVEC_HXX_
#define _IOVEC_HXX_

#include <sys/types.h>

//! Memory vector used by the network layer
/**
 * @author Longda
 * @date   4/29/07
 *
 * This class represents a memory vector abstraction, which is the unit of work
 * for the network layer (implemented through the Net, Conn, and ConnMgr
 * classes). In addition to the normal buffer with its pointer and size, the
 * class has a number of members for keeping track of the current progress
 * of processing for the IoVec objects as the network layer works in
 * non-blocking mode.
 * <p>
 * An important design concept is the completion callback. As the vectors are 
 * processed asyncrhonously, the callers of the IoVec interface do not have
 * direct means of learning when the vector is actually completed except
 * by polling on a status flag. This would have negated the benefits of
 * non-blocking asynchronous communication. Instead, in this design
 * callbacks are used for notifying the user of a vector, when the 
 * processing of this vector is compledted.
 */
class IoVec {
public:
    //! Structure representing a basic memory vector
    /**
     * This structure is similar to struct iovec defined in sys/uio.h and
     * used by the scatter/gather I/O calls, such as writev() and readv(). 
     */
    struct vec_t {
        void *base;     //!< pointer of vector buffer
        int   size;    //!< size of memory vector
    };

    //! Enumeration for vector status
    /**
     * Vector status is one of the input parameters to the completion callabck.
     * The vector status gives additional context to the callback to perform
     * necessary processing in the callback.
     */ 
    typedef enum {
        DONE = 0,       //!< vector is completed successfully
        ERROR,          //!< an error has occured during vector processing
        CLEANUP         //!< the vector is being cleaned up
    } state_t;

    //! Enumeration for type of vector buffer memory
    /**
     * Knwoing the type of memory allocation in the callback helps with
     * determining whether the memory will be freed in the callback or not.
     * The latter case is applicable when the buffer is allocated by the 
     * user code.
     */
    typedef enum {
        SYS_ALLOC = 0,  //!< System allocated
        USER_ALLOC      //!< User allocated
    } alloc_t;

    //! Enumeration for callback return status
    /**
     * The registered with the IoVec callback returns status code indicating
     * to the processing code how the callback completed.
     */
    enum {
        CB_SUCCESS = 0, //!< callback completed successfully
        CB_ERROR        //!< callback failed with an error
    };

    //! Type definition of the IoVec callback
    /**
     * The callback is registered by the IoVec user.
     * 
     * @param[in]   iov     pointer to the completed vector
     * @param[in]   param   opaque user context
     * @param[in]   state   vector state
     * @return      callback completion status
     * @pre     communication processing for the vector is completed
     */
    typedef int (*callback_t)(IoVec *iov, void *param, IoVec::state_t state);

public:
    //! Constructor 
    /**
     * @param[in]   alloc   allocation type of vector memory (optional)
     */
    IoVec(IoVec::alloc_t alloc = IoVec::SYS_ALLOC);

    //! Constructor 
    /**
     * @param[in]   base    vector buffer pointer
     * @param[in]   size    vector size
     * @param[in]   alloc   allocation type of vector memory (optional)
     */
    IoVec(void *base, size_t size, IoVec::alloc_t alloc = IoVec::SYS_ALLOC);

    //! Constructor 
    /**
     * @param[in]   base    vector buffer pointer
     * @param[in]   size    vector size
     * @param[in]   callback completion callback
     * @param[in]   param   callback parameter
     * @param[in]   alloc   allocation type of vector memory (optional)
     */
    IoVec(void *base, size_t size, IoVec::callback_t callback, void *param,
            IoVec::alloc_t alloc = IoVec::SYS_ALLOC);

    //! Constructor 
    /**
     * @param[in]   vec     vector {base,size} structure
     * @param[in]   alloc   allocation type of vector memory (optional)
     */
    IoVec(vec_t *vec, IoVec::alloc_t alloc = IoVec::SYS_ALLOC);

    //! Destructor
    /**
     * Default destructor
     */
    ~IoVec();

    /**
     * Free all internal memory, this is often used failure case
     */
    void cleanup();

    //! Set vector base 
    /**
     * @param[in]   base    vector buffer pointer
     */
    void setBase(void *base);

    //! Set vector size 
    /**
     * @param[in]   size    vector size
     */
    void setSize(size_t size);

    //! Set number of transferred bytes
    /**
     * @param[in]   deon    number of transferred bytes
     */
    void setXferred(size_t done);

    //! Set vector base and size 
    /**
     * @param[in]   base    vector buffer pointer
     * @param[in]   size    vector size
     */
    void setVec(void *base, size_t size);

    //! Set vector base and size
    /**
     * @param[in]   vec     vector {base,size} structure
     */
    void setVec(IoVec::vec_t *vec);

    //! Increment number of transferred bytes 
    /**
     * @param[in]   inc     number of bytes
     */
    void incXferred(size_t inc);

    //! Set completion callback
    /**
     * The callback is a user defined function. The parameter is an
     * opaque value (e.g., pointer to a data structure), which is passed
     * to the callback when the vector is completed.
     *
     * @param[in]   callback    completion callback
     * @param[in]   param       callback parameter
     */
    void setCallback(IoVec::callback_t callback, void *param);

    //! Reset vector members to initial state
    /**
     * This method prepares the vector for reuse by resetting all members to
     * their initial state, keeping the registered callback and its parameter.
     *
     * @post all members are set to their original values as set in constructor
     */
    void reset();

    //! Get vectors base
    /**
     * @return vector base pointer
     */
    void* getBase();

    //! Get vector size
    /**
     * @return vector size
     */
    size_t getSize();

    //! Get number of transferred bytes
    /**
     * @return number of transferred bytes
     */
    size_t getXferred();

    //! Get vector base and size
    /**
     * @return a vec_t structure with vector's base and size
     */
    IoVec::vec_t getVec();

    //! Get completion callback
    /**
     * @return the completion callback registered with vector
     */
    IoVec::callback_t getCallback();

    //! Get callback parameter
    /**
     * @return callback parameter
     */
    void* getCallbackParam();

    //! Get current pointer
    /**
     * As the vector is being processed, data is being read from or written to
     * its buffer. Each read/write operation moved the current pointer with
     * a certain number of bytes. This method returns the current position
     * of the working pointer. In the beginning, the current pointer is the
     * same as the base pointer.
     *
     * @return pointer to current location
     */
    void* curPtr();

    //! Get remaining number of bytes 
    /**
     * The remining number of bytes are the differnce between the 
     * vector size and the number of transferred bytes.
     *
     * @return remaining number of bytes from this vector
     */
    size_t remain(); 

    //! Is the vector done
    /**
     * The vector is done when the number of remaining bytes is 0.
     *
     * @return true if the vector is completed, false otherwise
     */
    bool done();

    //! Has the vector been started
    /**
     * The vector is started if a number of bytes greater than 0 has been 
     * transferred.
     *
     * @return true if vector has been started, false otherwise
     */
    bool started();

    //! Set allocation type
    /**
     * @param[in]   alloc   allocation type for memory buffer
     */
    void setAllocType(IoVec::alloc_t alloc);

    //! Get allocation type
    /**
     * @return buffer allocation type
     */
    IoVec::alloc_t getAllocType();

private:
    void *base;             //!< memory vectror base
    size_t size;            //!< vector size in bytes
    size_t xferred;         //!< number of transferred bytes
    callback_t callback;    //!< user registered vector completion callback
    void* cbParam;          //!< opaque callback context
    alloc_t alloc;          //!< type of memory allocation
};

#endif // _IOVEC_HXX_
