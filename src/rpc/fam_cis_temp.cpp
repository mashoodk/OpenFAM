/*
 * fam_rpc_service_impl.cpp
 * Copyright (c) 2019 Hewlett Packard Enterprise Development, LP. All rights
 * reserved. Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 *    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * See https://spdx.org/licenses/BSD-3-Clause
 *
 */
#include "fam_cis_temp.h"
#include "common/fam_memserver_profile.h"
#include <thread>

#include <boost/atomic.hpp>

#include <chrono>
#include <iomanip>
#include <string.h>
#include <unistd.h>

using namespace std;
using namespace chrono;
namespace openfam {
MEMSERVER_PROFILE_START(RPC_SERVICE)
#ifdef MEMSERVER_PROFILE
#define CIS_PROFILE_START_OPS()                                        \
    {                                                                          \
        Profile_Time start = RPC_SERVICE_get_time();

#define CIS_PROFILE_END_OPS(apiIdx)                                    \
    Profile_Time end = RPC_SERVICE_get_time();                                 \
    Profile_Time total = RPC_SERVICE_time_diff_nanoseconds(start, end);        \
    MEMSERVER_PROFILE_ADD_TO_TOTAL_OPS(RPC_SERVICE, prof_##apiIdx, total)      \
    }
#define CIS_PROFILE_DUMP() cis_profile_dump()
#else
#define CIS_PROFILE_START_OPS()
#define CIS_PROFILE_END_OPS(apiIdx)
#define CIS_PROFILE_DUMP()
#endif

void cis_profile_dump() {
    MEMSERVER_PROFILE_END(CIS_SERVICE);
    MEMSERVER_DUMP_PROFILE_BANNER(CIS_SERVICE)
#undef MEMSERVER_COUNTER
#define MEMSERVER_COUNTER(name)                                                \
    MEMSERVER_DUMP_PROFILE_DATA(RPC_SERVICE, name, prof_##name)
#include "rpc/rpc_service_counters.tbl"

#undef MEMSERVER_COUNTER
#define MEMSERVER_COUNTER(name)                                                \
    MEMSERVER_PROFILE_TOTAL(RPC_SERVICE, prof_##name)
#include "rpc/rpc_service_counters.tbl"
    MEMSERVER_DUMP_PROFILE_SUMMARY(RPC_SERVICE)
}
#if 0
void Fam_CIS_temp::progress_thread() {
    if (libfabricProgressMode == FI_PROGRESS_MANUAL) {
        while (1) {
            if (!haltProgress)
                famOps->quiet();
            else
                break;
        }
    }
}
void Fam_CIS_temp::rpc_service_initialize(
    char *name, char *service, char *provider, Memserver_Allocator *memAlloc) {
    ostringstream message;
    message << "Error while initializing RPC service : ";
    numClients = 0;
    shouldShutdown = false;
    allocator = memAlloc;
    MEMSERVER_PROFILE_INIT(RPC_SERVICE)
    MEMSERVER_PROFILE_START_TIME(RPC_SERVICE)
    fiMrs = NULL;
    fenceMr = 0;

    famOps =
        new Fam_Ops_Libfabric(name, service, true, provider,
                              FAM_THREAD_MULTIPLE, NULL, FAM_CONTEXT_DEFAULT);
    int ret = famOps->initialize();
    if (ret < 0) {
        message << "famOps initialization failed";
        throw Memserver_Exception(OPS_INIT_FAILED, message.str().c_str());
    }
    struct fi_info *fi = famOps->get_fi();
    if (fi->domain_attr->control_progress == FI_PROGRESS_MANUAL ||
        fi->domain_attr->data_progress == FI_PROGRESS_MANUAL) {
        libfabricProgressMode = FI_PROGRESS_MANUAL;
    }
    ret = register_fence_memory();
    if (ret < 0) {
        message << "Failed to register memory for fence operation";
        throw Memserver_Exception(FENCE_REG_FAILED, message.str().c_str());
    }
    for (int i = 0; i < CAS_LOCK_CNT; i++) {
        (void)pthread_mutex_init(&casLock[i], NULL);
    }
    if (libfabricProgressMode == FI_PROGRESS_MANUAL) {
        haltProgress = false;
        progressThread =
            std::thread(&Fam_CIS_temp::progress_thread, this);
    }
}

void Fam_CIS_temp::rpc_service_finalize() {
    allocator->memserver_allocator_finalize();
    deregister_fence_memory();
    for (int i = 0; i < CAS_LOCK_CNT; i++) {
        (void)pthread_mutex_destroy(&casLock[i]);
    }
    famOps->finalize();
}

#endif
Fam_CIS_temp::Fam_CIS_temp(Memserver_Allocator *memAlloc) {
	allocator = memAlloc;
}

Fam_CIS_temp::~Fam_CIS_temp() {
#if 0
    if (libfabricProgressMode == FI_PROGRESS_MANUAL) {
        haltProgress = true;
        progressThread.join();
    }
    famOps->finalize();
    delete famOps;
#endif
    delete allocator;
    delete metadataManager;
}
#if 0
::grpc::Status
Fam_CIS_temp::signal_start(::grpc::ServerContext *context,
                                   const ::Fam_Request *request,
                                   ::Fam_Start_Response *response) {
    __sync_add_and_fetch(&numClients, 1);

    size_t addrSize = famOps->get_addr_size();
    void *addr = famOps->get_addr();

    response->set_addrnamelen(addrSize);
    int count = (int)(addrSize / sizeof(uint32_t));
    for (int ndx = 0; ndx < count; ndx++) {
        response->add_addrname(*((uint32_t *)addr + ndx));
    }

    // Only if addrSize is not multiple of 4 (fixed32)
    int lastBytesCount = 0;
    uint32_t lastBytes = 0;

    lastBytesCount = (int)(addrSize % sizeof(uint32_t));
    if (lastBytesCount > 0) {
        memcpy(&lastBytes, ((uint32_t *)addr + count), lastBytesCount);
        response->add_addrname(lastBytes);
    }

    return ::grpc::Status::OK;
}

::grpc::Status
Fam_CIS_temp::signal_termination(::grpc::ServerContext *context,
                                         const ::Fam_Request *request,
                                         ::Fam_Response *response) {
    __sync_add_and_fetch(&numClients, -1);
    return ::grpc::Status::OK;
}
::grpc::Status
Fam_CIS_temp::reset_profile(::grpc::ServerContext *context,
                                    const ::Fam_Request *request,
                                    ::Fam_Response *response) {

    MEMSERVER_PROFILE_INIT(RPC_SERVICE)
    MEMSERVER_PROFILE_START_TIME(RPC_SERVICE)
    fabric_reset_profile();
    allocator->reset_profile();
    return ::grpc::Status::OK;
}
#endif
void Fam_CIS_temp::dump_profile() {
    CIS_PROFILE_DUMP();
    //fabric_dump_profile();
    //allocator->dump_profile();
}
#if 0
::grpc::Status
Fam_CIS_temp::generate_profile(::grpc::ServerContext *context,
                                       const ::Fam_Request *request,
                                       ::Fam_Response *response) {
#ifdef MEMSERVER_PROFILE
    kill(getpid(), SIGINT);
#endif
    return ::grpc::Status::OK;
}

#endif
void
Fam_CIS_temp::create_region(string name, uint64_t &regionId, size_t nbytes,
                                    mode_t permission, uint32_t uid, uint32_t gid) {
    CIS_PROFILE_START_OPS()
    Fam_Region_Metadata region;
    ostringstream message;

    // Check if the name size is bigger than MAX_KEY_LEN supported
    if (name.size() > metadataManager->metadata_maxkeylen()) {
        message << "Name too long";
        throw Memserver_Exception(REGION_NAME_TOO_LONG, message.str().c_str());
    }

    int ret = metadataManager->metadata_find_region(name, region);
    if (ret == META_NO_ERROR) {
        message << "Region already exist";
	throw Memserver_Exception(REGION_EXIST, message.str().c_str());
    }
    try {
        allocator->create_region(
            name, regionId, nbytes, permission, uid, gid);
    } catch (Memserver_Exception &e) {
        //response->set_errorcode(e.fam_error());
        //response->set_errormsg(e.fam_error_msg());
	throw;
        //return ::grpc::Status::OK;
    }

    // Register the region into metadata service
    region.regionId = regionId;
    strncpy(region.name, name.c_str(), metadataManager->metadata_maxkeylen());
    region.offset = INVALID_OFFSET;
    region.perm = permission;
    region.uid = uid;
    region.gid = gid;
    region.size = nbytes;
    ret = metadataManager->metadata_insert_region(regionId, name, &region);
    if (ret != META_NO_ERROR) {
	    // TODO: call allcator->destroy region
    }
    CIS_PROFILE_END_OPS(create_region);
    // Return status OK
    return;
}

void
Fam_CIS_temp::destroy_region(uint64_t regionId, uint32_t uid,
                                     uint32_t gid) {
    CIS_PROFILE_START_OPS()
    ostringstream message;

    // Check with metadata service if the region exist, if not return error
    Fam_Region_Metadata region;
    int ret = metadataManager->metadata_find_region(regionId, region);
    if (ret != META_NO_ERROR) {
        message << "Region does not exist";
        throw Memserver_Exception(REGION_NOT_FOUND, message.str().c_str());
    }

    // Check if calling PE user is owner. If not, check with
    // metadata service if the calling PE has the write
    // permission to destroy region, if not return error
    if (uid != region.uid) {
        bool isPermitted = metadataManager->metadata_check_permissions(
            &region, META_REGION_ITEM_WRITE, uid, gid);
        if (!isPermitted) {
            message << "Destroying region is not permitted";
            throw Memserver_Exception(DESTROY_REGION_NOT_PERMITTED,
                                      message.str().c_str());
        }
    }

    try {
        allocator->destroy_region(regionId, uid,
                                  gid);
    } catch (Memserver_Exception &e) {
	    throw;
        //response->set_errorcode(e.fam_error());
        //response->set_errormsg(e.fam_error_msg());
        //return ::grpc::Status::OK;
    }

    // TODO: Validate this
    // remove region from metadata service.
    // metadata_delete_region() is called before DestroyHeap() as
    // cached KVS is freed in metadata_delete_region and calling
    // metadata_delete_region after DestroyHeap will result in SIGSEGV.

    ret = metadataManager->metadata_delete_region(regionId);
    if (ret != META_NO_ERROR) {
        message << "Can not remove region from metadata service";
        throw Memserver_Exception(REGION_NOT_REMOVED, message.str().c_str());
    }


    CIS_PROFILE_END_OPS(destroy_region);
    // Return status OK
    return;
}

void
Fam_CIS_temp::resize_region(uint64_t regionId, uint32_t uid, uint32_t gid,
                                     size_t nbytes) {

    CIS_PROFILE_START_OPS()
    ostringstream message;

    // Check with metadata service if the region exist, if not return error
    Fam_Region_Metadata region;
    int ret = metadataManager->metadata_find_region(regionId, region);
    if (ret != META_NO_ERROR) {
        message << "Region does not exist";
        throw Memserver_Exception(REGION_NOT_FOUND, message.str().c_str());
    }

    bool isPermitted = metadataManager->metadata_check_permissions(
        &region, META_REGION_ITEM_WRITE, uid, gid);
    if (!isPermitted) {
        message << "Region resize not permitted";
        throw Memserver_Exception(REGION_RESIZE_NOT_PERMITTED,
                                  message.str().c_str());
    }
    
    try {
        allocator->resize_region(regionId, uid,
                                 gid, nbytes);
    } catch (Memserver_Exception &e) {
        //response->set_errorcode(e.fam_error());
        //response->set_errormsg(e.fam_error_msg());
        throw;
    }

    region.size = nbytes;
    // Update the size in the metadata service
    ret = metadataManager->metadata_modify_region(regionId, &region);
    if (ret != META_NO_ERROR) {
        message << "Can not modify metadata service, ";
        throw Memserver_Exception(REGION_NOT_MODIFIED, message.str().c_str());
    }

    CIS_PROFILE_END_OPS(resize_region);

    // Return status OK
    //return ::grpc::Status::OK;
    throw;
}

void
Fam_CIS_temp::allocate(string name, uint64_t regionId, size_t nbytes,
                               uint64_t &offset, mode_t permission,
                               uint32_t uid, uint32_t gid,
			       Fam_DataItem_Metadata &dataitem,
			       void *&localPointer) {
    CIS_PROFILE_START_OPS()
    ostringstream message;
    //void *localPointer;
    int ret = 0;

    // Check if the name size is bigger than MAX_KEY_LEN supported
    if (name.size() > metadataManager->metadata_maxkeylen()) {
        message << "Name too long";
        throw Memserver_Exception(DATAITEM_NAME_TOO_LONG,
                                  message.str().c_str());
    }

    // Check with metadata service if the region exist, if not return error
    Fam_Region_Metadata region;
    ret = metadataManager->metadata_find_region(regionId, region);
    if (ret != META_NO_ERROR) {
        message << "Region does not exist";
        throw Memserver_Exception(REGION_NOT_FOUND, message.str().c_str());
    }

    // Check if calling PE user is owner. If not, check with
    // metadata service if the calling PE has the write
    // permission to create dataitem in that region, if not return error
    if (uid != region.uid) {
        bool isPermitted = metadataManager->metadata_check_permissions(
            &region, META_REGION_ITEM_WRITE, uid, gid);
        if (!isPermitted) {
            message << "Allocation of dataitem is not permitted";
            throw Memserver_Exception(DATAITEM_ALLOC_NOT_PERMITTED,
                                      message.str().c_str());
        }
    }

    // Check with metadata service if data item with the requested name
    // is already exist, if exists return error
    if (name != "") {
        ret = metadataManager->metadata_find_dataitem(name, regionId, dataitem);
        if (ret == META_NO_ERROR) {
            message << "Dataitem with the name provided already exist";
            throw Memserver_Exception(DATAITEM_EXIST, message.str().c_str());
        }
    }
	 
        try {
            allocator->allocate(name, regionId,
                                nbytes, offset,
                                permission, uid,
                                gid, dataitem, localPointer);
        } catch (Memserver_Exception &e) {
            //response->set_errorcode(e.fam_error());
            //response->set_errormsg(e.fam_error_msg());            
	    //return ::grpc::Status::OK;
	    throw;
        }
    //}

    uint64_t dataitemId = offset / MIN_OBJ_SIZE;
    dataitem.regionId = regionId;
    strncpy(dataitem.name, name.c_str(), metadataManager->metadata_maxkeylen());
    dataitem.offset = offset;
    dataitem.perm = permission;
    dataitem.gid = gid;
    dataitem.uid = uid;
    dataitem.size = nbytes;
    if (name == "")
        ret = metadataManager->metadata_insert_dataitem(dataitemId, regionId,
                                                        &dataitem);
    else
        ret = metadataManager->metadata_insert_dataitem(dataitemId, regionId,
                                                        &dataitem, name);
    if (ret != META_NO_ERROR) {
        //TODO: Call deallocate
	message << "Can not insert dataitem into metadata service";
        throw Memserver_Exception(DATAITEM_NOT_INSERTED, message.str().c_str());
    }

    // Return status OK
    return;
}

void
Fam_CIS_temp::deallocate(uint64_t regionId, uint64_t offset,
                                 uint32_t uid, uint32_t gid) {
    CIS_PROFILE_START_OPS()
    ostringstream message;

    // Check with metadata service if data item with the requested name
    // is already exist, if not return error
    uint64_t dataitemId = offset / MIN_OBJ_SIZE;
    Fam_DataItem_Metadata dataitem;
    int ret =
        metadataManager->metadata_find_dataitem(dataitemId, regionId, dataitem);
    if (ret != META_NO_ERROR) {
        message << "Dataitem does not exist";
        throw Memserver_Exception(DATAITEM_NOT_FOUND, message.str().c_str());
    }

    // Check if calling PE user is owner. If not, check with
    // metadata service if the calling PE has the write
    // permission to destroy region, if not return error
    if (uid != dataitem.uid) {
        bool isPermitted = metadataManager->metadata_check_permissions(
            &dataitem, META_REGION_ITEM_WRITE, uid, gid);
        if (!isPermitted) {
            message << "Deallocation of dataitem is not permitted";
            throw Memserver_Exception(DATAITEM_DEALLOC_NOT_PERMITTED,
                                      message.str().c_str());
        }
    }

    // Remove data item from metadata service
    ret = metadataManager->metadata_delete_dataitem(dataitemId, regionId);
    if (ret != META_NO_ERROR) {
        message << "Can not remove dataitem from metadata service";
        throw Memserver_Exception(DATAITEM_NOT_REMOVED, message.str().c_str());
    }

    try {
        allocator->deallocate(regionId, offset,
                              uid, gid);
    } catch (Memserver_Exception &e) {
        //response->set_errorcode(e.fam_error());
        //response->set_errormsg(e.fam_error_msg());
        //return ::grpc::Status::OK;
	throw;
    }

    CIS_PROFILE_END_OPS(deallocate);

    // Return status OK
    return;
}

void Fam_CIS_temp::change_region_permission(
    uint64_t regionId,mode_t permission,
    uint32_t uid, uint32_t gid) {
    CIS_PROFILE_START_OPS()

    ostringstream message;
    message << "Error While changing region permission : ";
    // Check with metadata service if region with the requested Id
    // is already exist, if not return error
    Fam_Region_Metadata region;
    int ret = metadataManager->metadata_find_region(regionId, region);
    if (ret != META_NO_ERROR) {
        message << "Region does not exist";
        throw Memserver_Exception(REGION_NOT_FOUND, message.str().c_str());
    }

    // Check with metadata service if the calling PE has the permission
    // to modify permissions of the region, if not return error
    if (uid != region.uid) {
        message << "Region permission modify not permitted";
        throw Memserver_Exception(REGION_PERM_MODIFY_NOT_PERMITTED,
                                  message.str().c_str());
    }

    // Update the permission of region with metadata service
    region.perm = permission;
    ret = metadataManager->metadata_modify_region(regionId, &region);

    CIS_PROFILE_END_OPS(change_region_permission);

    // Return status OK
    return;
}

void Fam_CIS_temp::change_dataitem_permission(uint64_t regionId,
                                              uint64_t offset,
                                              mode_t permission,
                                              uint32_t uid,
                                              uint32_t gid) {
    CIS_PROFILE_START_OPS()
    ostringstream message;
    message << "Error While changing dataitem permission : ";
    // Check with metadata service if region with the requested Id
    // is already exist, if not return error
    uint64_t dataitemId = offset / MIN_OBJ_SIZE;
    Fam_DataItem_Metadata dataitem;
    int ret =
        metadataManager->metadata_find_dataitem(dataitemId, regionId, dataitem);
    if (ret != META_NO_ERROR) {
        message << "Dataitem does not exist";
        throw Memserver_Exception(DATAITEM_NOT_FOUND, message.str().c_str());
    }

    // Check with metadata service if the calling PE has the permission
    // to modify permissions of the region, if not return error
    if (uid != dataitem.uid) {
        message << "Dataitem permission modify not permitted";
        throw Memserver_Exception(ITEM_PERM_MODIFY_NOT_PERMITTED,
                                  message.str().c_str());
    }

    // Update the permission of region with metadata service
    dataitem.perm = permission;
    ret = metadataManager->metadata_modify_dataitem(dataitemId, regionId,
                                                    &dataitem);

    CIS_PROFILE_END_OPS(change_dataitem_permission);
    // Return status OK
    return;
}

void
Fam_CIS_temp::lookup_region(string name, uint32_t uid, uint32_t gid,
                                    Fam_Region_Metadata &region) {
    CIS_PROFILE_START_OPS()
    ostringstream message;
    //Fam_Region_Metadata region;

    message << "Error While locating region : ";
    int ret = metadataManager->metadata_find_region(name, region);
    if (ret != META_NO_ERROR) {
        message << "could not find the region";
        throw Memserver_Exception(REGION_NOT_FOUND, message.str().c_str());
    }

    if (uid != region.uid) {       
	          metadata_region_item_op_t opFlag = META_REGION_ITEM_READ;
        bool isPermitted = metadataManager->metadata_check_permissions(&region, opFlag, uid, gid);
        if (!isPermitted) {
            message << "could not find the region " << name;     
            throw Memserver_Exception(REGION_NOT_FOUND, message.str().c_str());
        }

    }
    CIS_PROFILE_END_OPS(lookup_region);
    return ;
}

void
Fam_CIS_temp::lookup(string itemName, string regionName,
                     uint32_t uid, uint32_t gid,
                     Fam_DataItem_Metadata &dataitem) {
    CIS_PROFILE_START_OPS()
    ostringstream message;
    message << "Error While locating dataitem : ";
    int ret =
        metadataManager->metadata_find_dataitem(itemName, regionName, dataitem);
    if (ret != META_NO_ERROR) {
        message << "could not find the dataitem" << itemName << " "
                << regionName << " " << ret;
        throw Memserver_Exception(DATAITEM_NOT_FOUND, message.str().c_str());
    }

    if (uid != dataitem.uid) {
        metadata_region_item_op_t opFlag = META_REGION_ITEM_READ;
        bool isPermitted = metadataManager->metadata_check_permissions(&dataitem, opFlag, uid, gid);
	if (!isPermitted) {
            message << "could not find the dataitem" << itemName << " "
                    << regionName;
            throw Memserver_Exception(DATAITEM_NOT_FOUND, message.str().c_str());
	}
    }

        //check_dataitem_permission(dataitem, 0, uid,gid);
    // Return status OK
    CIS_PROFILE_END_OPS(lookup);
    return;
}

void Fam_CIS_temp::check_permission_get_region_info(uint64_t regionId,         
                                              uint32_t uid, uint32_t gid) {

    CIS_PROFILE_START_OPS()	  
    Fam_Region_Metadata region; 
    ostringstream message;
    message << "Error While locating region : ";
    int ret = metadataManager->metadata_find_region(regionId, region);
    if (ret != META_NO_ERROR) {
        message << "could not find the region";
        throw Memserver_Exception(REGION_NOT_FOUND, message.str().c_str());
    }
        metadata_region_item_op_t opFlag = META_REGION_ITEM_READ;
        ret = metadataManager->metadata_check_permissions(&region, opFlag, uid, gid);
	if (ret != META_NO_ERROR) {
        message << "could not find the region" << regionId << " "
                 << ret;
        throw Memserver_Exception(REGION_NOT_FOUND, message.str().c_str());
    }

    CIS_PROFILE_END_OPS(check_permission_get_region_info);
    return;
}

void Fam_CIS_temp::check_permission_get_item_info(
    uint64_t regionId, uint64_t offset,
    uint32_t uid, uint32_t gid) {

    CIS_PROFILE_START_OPS()

    ostringstream message;
    Fam_DataItem_Metadata dataitem;
    message << "Error While locating dataitem : ";
    uint64_t dataitemId = offset / MIN_OBJ_SIZE;
    int ret =
        metadataManager->metadata_find_dataitem(dataitemId, regionId, dataitem);
    if (ret != META_NO_ERROR) {
        message << "could not find the dataitem";
        throw Memserver_Exception(DATAITEM_NOT_FOUND, message.str().c_str());
    }

    CIS_PROFILE_END_OPS(check_permission_get_item_info);

    // Return status OK
    return; 
}

#if 0
::grpc::Status Fam_CIS_temp::copy(::grpc::ServerContext *context,
                                          const ::Fam_Copy_Request *request,
                                          ::Fam_Copy_Response *response) {
    return ::grpc::Status::OK;
}

uint64_t Fam_CIS_temp::generate_access_key(uint64_t regionId,
                                                   uint64_t dataitemId,
                                                   bool permission) {
    uint64_t key = 0;

    key |= (regionId & REGIONID_MASK) << REGIONID_SHIFT;
    key |= (dataitemId & DATAITEMID_MASK) << DATAITEMID_SHIFT;
    key |= permission;

    return key;
}

int Fam_CIS_temp::register_fence_memory() {

    int ret;
    fid_mr *mr = 0;
    uint64_t key = FAM_FENCE_KEY;
    void *localPointer;
    size_t len = (size_t)sysconf(_SC_PAGESIZE);
    int fd = -1;

    localPointer = mmap(NULL, len, PROT_WRITE, MAP_SHARED | MAP_ANON, fd, 0);
    if (localPointer == MAP_FAILED) {
    }

    // register the memory location with libfabric
    if (fenceMr == 0) {
        ret = fabric_register_mr(localPointer, len, &key, famOps->get_domain(),
                                 1, mr);
        if (ret < 0) {
            cout << "error: memory register failed" << endl;
            return ITEM_REGISTRATION_FAILED;
        }
        fenceMr = mr;
    }
    // Return status OK
    return 0;
}

int Fam_CIS_temp::register_memory(Fam_DataItem_Metadata dataitem,
                                          void *&localPointer, uint32_t uid,
                                          uint32_t gid, uint64_t &key) {
    uint64_t dataitemId = dataitem.offset / MIN_OBJ_SIZE;
    if (fiMrs == NULL)
        fiMrs = famOps->get_fiMrs();
    fid_mr *mr = 0;
    int ret = 0;
    uint64_t mrkey = 0;
    bool rwflag;
    Fam_Region_Map_t *fiRegionMap = NULL;
    Fam_Region_Map_t *fiRegionMapDiscard = NULL;

    if (!localPointer) {
        localPointer =
            allocator->get_local_pointer(dataitem.regionId, dataitem.offset);
    }

    if (allocator->check_dataitem_permission(dataitem, 1, uid, gid)) {
        key = mrkey = generate_access_key(dataitem.regionId, dataitemId, 1);
        rwflag = 1;
    } else if (allocator->check_dataitem_permission(dataitem, 0, uid, gid)) {
        key = mrkey = generate_access_key(dataitem.regionId, dataitemId, 0);
        rwflag = 0;
    } else {
        cout << "error: Not permitted to register dataitem" << endl;
        return NOT_PERMITTED;
    }

    // register the data item with required permission with libfabric
    // Start by taking a readlock on fiMrs
    pthread_rwlock_rdlock(famOps->get_mr_lock());
    auto regionMrObj = fiMrs->find(dataitem.regionId);
    if (regionMrObj == fiMrs->end()) {
        // Create a RegionMap
        fiRegionMap = (Fam_Region_Map_t *)calloc(1, sizeof(Fam_Region_Map_t));
        fiRegionMap->regionId = dataitem.regionId;
        fiRegionMap->fiRegionMrs = new std::map<uint64_t, fid_mr *>();
        pthread_rwlock_init(&fiRegionMap->fiRegionLock, NULL);
        // RegionMap not found, release read lock, take a write lock
        pthread_rwlock_unlock(famOps->get_mr_lock());
        pthread_rwlock_wrlock(famOps->get_mr_lock());
        // Check again if regionMap added by another thread.
        regionMrObj = fiMrs->find(dataitem.regionId);
        if (regionMrObj == fiMrs->end()) {
            // Add the fam region map into fiMrs
            fiMrs->insert({dataitem.regionId, fiRegionMap});
        } else {
            // Region Map already added by another thread,
            // discard the one created here.
            fiRegionMapDiscard = fiRegionMap;
            fiRegionMap = regionMrObj->second;
        }
    } else {
        fiRegionMap = regionMrObj->second;
    }

    // Take a writelock on fiRegionMap
    pthread_rwlock_wrlock(&fiRegionMap->fiRegionLock);
    // Release lock on fiMrs
    pthread_rwlock_unlock(famOps->get_mr_lock());

    // Delete the discarded region map here.
    if (fiRegionMapDiscard != NULL) {
        delete fiRegionMapDiscard->fiRegionMrs;
        free(fiRegionMapDiscard);
    }

    auto mrObj = fiRegionMap->fiRegionMrs->find(key);
    if (mrObj == fiRegionMap->fiRegionMrs->end()) {
        ret = fabric_register_mr(localPointer, dataitem.size, &mrkey,
                                 famOps->get_domain(), rwflag, mr);
        if (ret < 0) {
            pthread_rwlock_unlock(&fiRegionMap->fiRegionLock);
            cout << "error: memory register failed" << endl;
            return ITEM_REGISTRATION_FAILED;
        }

        fiRegionMap->fiRegionMrs->insert({key, mr});
    } else {
        mrkey = fi_mr_key(mrObj->second);
    }
    // Always return mrkey, which might be different than key.
    key = mrkey;

    pthread_rwlock_unlock(&fiRegionMap->fiRegionLock);
    // Return status OK
    return 0;
}

int Fam_CIS_temp::deregister_fence_memory() {

    int ret = 0;
    if (fenceMr != 0) {
        ret = fabric_deregister_mr(fenceMr);
        if (ret < 0) {
            cout << "error: memory deregister failed" << endl;
            return ITEM_DEREGISTRATION_FAILED;
        }
    }
    fenceMr = 0;
    return 0;
}

int Fam_CIS_temp::deregister_memory(uint64_t regionId,
                                            uint64_t offset) {
    CIS_PROFILE_START_OPS()

    int ret = 0;
    uint64_t dataitemId = offset / MIN_OBJ_SIZE;
    uint64_t rKey = generate_access_key(regionId, dataitemId, 0);
    uint64_t rwKey = generate_access_key(regionId, dataitemId, 1);
    Fam_Region_Map_t *fiRegionMap;

    if (fiMrs == NULL)
        fiMrs = famOps->get_fiMrs();

    // Take read lock on fiMrs
    pthread_rwlock_rdlock(famOps->get_mr_lock());
    auto regionMrObj = fiMrs->find(regionId);
    if (regionMrObj == fiMrs->end()) {
        pthread_rwlock_unlock(famOps->get_mr_lock());
        return 0;
    } else {
        fiRegionMap = regionMrObj->second;
    }

    // Take a writelock on fiRegionMap
    pthread_rwlock_wrlock(&fiRegionMap->fiRegionLock);
    // Release lock on fiMrs
    pthread_rwlock_unlock(famOps->get_mr_lock());

    auto rMr = fiRegionMap->fiRegionMrs->find(rKey);
    auto rwMr = fiRegionMap->fiRegionMrs->find(rwKey);
    if (rMr != fiRegionMap->fiRegionMrs->end()) {
        ret = fabric_deregister_mr(rMr->second);
        if (ret < 0) {
            pthread_rwlock_unlock(&fiRegionMap->fiRegionLock);
            cout << "error: memory deregister failed" << endl;
            return ITEM_DEREGISTRATION_FAILED;
        }
        fiRegionMap->fiRegionMrs->erase(rMr);
    }

    if (rwMr != fiRegionMap->fiRegionMrs->end()) {
        ret = fabric_deregister_mr(rwMr->second);
        if (ret < 0) {
            pthread_rwlock_unlock(&fiRegionMap->fiRegionLock);
            cout << "error: memory deregister failed" << endl;
            return ITEM_DEREGISTRATION_FAILED;
        }
        fiRegionMap->fiRegionMrs->erase(rwMr);
    }

    pthread_rwlock_unlock(&fiRegionMap->fiRegionLock);
    CIS_PROFILE_END_OPS(deregister_memory);
    return 0;
}

int Fam_CIS_temp::deregister_region_memory(uint64_t regionId) {
    int ret = 0;
    Fam_Region_Map_t *fiRegionMap;

    if (fiMrs == NULL)
        fiMrs = famOps->get_fiMrs();

    // Take write lock on fiMrs
    pthread_rwlock_wrlock(famOps->get_mr_lock());

    auto regionMrObj = fiMrs->find(regionId);
    if (regionMrObj == fiMrs->end()) {
        pthread_rwlock_unlock(famOps->get_mr_lock());
        return 0;
    } else {
        fiRegionMap = regionMrObj->second;
    }

    // Take a writelock on fiRegionMap
    pthread_rwlock_wrlock(&fiRegionMap->fiRegionLock);
    // Remove region map from fiMrs
    fiMrs->erase(regionMrObj);
    // Release lock on fiMrs
    pthread_rwlock_unlock(famOps->get_mr_lock());

    // Unregister all dataItem memory from region map
    for (auto mr : *(fiRegionMap->fiRegionMrs)) {
        ret = fabric_deregister_mr(mr.second);
        if (ret < 0) {
            cout << "destroy region<" << fiRegionMap->regionId
                 << ">: memory deregister failed with errno(" << ret << ")"
                 << endl;
        }
    }

    pthread_rwlock_unlock(&fiRegionMap->fiRegionLock);
    fiRegionMap->fiRegionMrs->clear();
    delete fiRegionMap->fiRegionMrs;
    free(fiRegionMap);

    return 0;
}
#endif
void
Fam_CIS_temp::acquire_CAS_lock(uint64_t offset) {
    int idx = LOCKHASH(offset);
    pthread_mutex_lock(&casLock[idx]);

    // Return status OK
    return;
}

void
Fam_CIS_temp::release_CAS_lock(uint64_t offset) {
    int idx = LOCKHASH(offset);
    pthread_mutex_unlock(&casLock[idx]);

    // Return status OK
    return;
}

} // namespace openfam
