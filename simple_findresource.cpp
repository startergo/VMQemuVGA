VMVirtIOGPU::gpu_resource* CLASS::findResource(uint32_t resource_id)
{
    // Simple, safe resource lookup with null pointer protection
    if (!m_resources) {
        return nullptr;
    }
    
    if (resource_id == 0) {
        return nullptr;
    }
    
    // Use resource lock if available
    if (m_resource_lock) {
        IOLockLock(m_resource_lock);
    }
    
    unsigned int count = m_resources->getCount();
    gpu_resource* found = nullptr;
    
    for (unsigned int i = 0; i < count; i++) {
        OSObject* obj = m_resources->getObject(i);
        if (!obj) {
            continue; // Skip null objects
        }
        
        gpu_resource* resource = (gpu_resource*)obj;
        if (!resource) {
            continue; // Skip null resources
        }
        
        if (resource->resource_id == resource_id) {
            found = resource;
            break;
        }
    }
    
    if (m_resource_lock) {
        IOLockUnlock(m_resource_lock);
    }
    
    return found;
}
