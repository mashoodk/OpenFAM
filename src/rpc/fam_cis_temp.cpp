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

void Fam_CIS_temp::memserver_allocator_finalize()
{
	allocator->memserver_allocator_finalize();
}

Fam_CIS_temp::Fam_CIS_temp(Memserver_Allocator *memAlloc) {
	allocator = memAlloc;
	metadataManager = FAM_Metadata_Manager::GetInstance();
}

Fam_CIS_temp::~Fam_CIS_temp() {
    delete allocator;
    delete metadataManager;
}

void
Fam_CIS_temp::reset_profile() {

    MEMSERVER_PROFILE_INIT(CIS)
    MEMSERVER_PROFILE_START_TIME(CIS)
    allocator->reset_profile();
    return;
}

void Fam_CIS_temp::dump_profile() {
    CIS_PROFILE_DUMP();
    //fabric_dump_profile();
    allocator->dump_profile();
}

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

    // remove region from metadata service.
    // metadata_delete_region() is called before DestroyHeap() as
    // cached KVS is freed in metadata_delete_region and calling
    // metadata_delete_region after DestroyHeap will result in SIGSEGV.

    ret = metadataManager->metadata_delete_region(regionId);
    if (ret != META_NO_ERROR) {
        message << "Can not remove region from metadata service";
        throw Memserver_Exception(REGION_NOT_REMOVED, message.str().c_str());
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
    return;
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
	message << "Can not insert dataitem into metadata service "<<ret;
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

/*
 * Check if the given uid/gid has read or rw permissions.
 */
bool Fam_CIS_temp::check_region_permission(Fam_Region_Metadata region,
                                                  bool op, uint32_t uid,
                                                  uint32_t gid) {
    metadata_region_item_op_t opFlag;
    if (op)
        opFlag = META_REGION_ITEM_RW;
    else
        opFlag = META_REGION_ITEM_READ;

    return (
        metadataManager->metadata_check_permissions(&region, opFlag, uid, gid));
}

/*
 * Check if the given uid/gid has read or rw permissions for
 * a given dataitem.
 */
bool Fam_CIS_temp::check_dataitem_permission(
    Fam_DataItem_Metadata dataitem, bool op, uint32_t uid, uint32_t gid) {

    metadata_region_item_op_t opFlag;
    if (op)
        opFlag = META_REGION_ITEM_RW;
    else
        opFlag = META_REGION_ITEM_READ;

    return (metadataManager->metadata_check_permissions(&dataitem, opFlag, uid,
                                                        gid));
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

/*
 * Region name lookup given the region name.
 * Returns the Fam_Region_Metadata for the given name
 */
void Fam_CIS_temp::get_region(string name, uint32_t uid, uint32_t gid,
                                    Fam_Region_Metadata &region) {
    ostringstream message;
    message << "Error While locating region : ";
    int ret = metadataManager->metadata_find_region(name, region);
    if (ret != META_NO_ERROR) {
        message << "could not find the region";
        throw Memserver_Exception(REGION_NOT_FOUND, message.str().c_str());
    }

    return;
}

/*
 * Region name lookup given the region id.
 * Returns the Fam_Region_Metadata for the given name
 */
void Fam_CIS_temp::get_region(uint64_t regionId, uint32_t uid,
                                    uint32_t gid, Fam_Region_Metadata &region) {
    ostringstream message;
    message << "Error While locating region : ";
    int ret = metadataManager->metadata_find_region(regionId, region);
    if (ret != META_NO_ERROR) {
        message << "could not find the region";
        throw Memserver_Exception(REGION_NOT_FOUND, message.str().c_str());
    }

    return;
}


/*
 * dataitem lookup for the given region and dataitem name.
 * Returns the Fam_Dataitem_Metadata for the given name
 */
void Fam_CIS_temp::get_dataitem(string itemName, string regionName,
                                      uint32_t uid, uint32_t gid,
                                      Fam_DataItem_Metadata &dataitem) {
    ostringstream message;
    message << "Error While locating dataitem : ";
    int ret =
        metadataManager->metadata_find_dataitem(itemName, regionName, dataitem);
    if (ret != META_NO_ERROR) {
        message << "could not find the dataitem";
        throw Memserver_Exception(DATAITEM_NOT_FOUND, message.str().c_str());
    }

    return;
}

/*
 * dataitem lookup for the given region name and offset.
 * Returns the Fam_Dataitem_Metadata for the given name
 */
void Fam_CIS_temp::get_dataitem(uint64_t regionId, uint64_t offset,
                                      uint32_t uid, uint32_t gid,
                                      Fam_DataItem_Metadata &dataitem) {
    ostringstream message;
    message << "Error While locating dataitem : ";
    uint64_t dataitemId = offset / MIN_OBJ_SIZE;
    int ret =
        metadataManager->metadata_find_dataitem(dataitemId, regionId, dataitem);
    if (ret != META_NO_ERROR) {
        message << "could not find the dataitem";
        throw Memserver_Exception(DATAITEM_NOT_FOUND, message.str().c_str());
    }

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

void *Fam_CIS_temp::get_local_pointer(uint64_t regionId, uint64_t offset) {
	return allocator->get_local_pointer(regionId, offset);
}

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
