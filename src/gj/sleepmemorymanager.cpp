#include "sleepmemorymanager.h"
#include "base.h"

struct SleepMemoryManager::Header
{
	union {
		unsigned char const m_id[4] = { 'R', 'M', '0', '0' };
		uint32_t m_idUint;
	};
	uint32_t m_chunkCount = 0;
};

struct SleepMemoryManager::Chunk
{
	union {
		unsigned char const m_id[4] = { 'R', 'M', 'C', '0' };
		uint32_t m_idUint;
	};
	
	uint32_t m_uid = 0;
	uint32_t m_size = 0;
	uint32_t m_allocatedSize = 0;
};

SleepMemoryManager::SleepMemoryManager( void *memory, uint32_t size )
: m_memory(memory)
, m_size(size)
{
  SER("SleepMemoryManager:mem address: 0x%x size:%d\n\r", (int32_t)memory, size);
}
	

void SleepMemoryManager::Clear()
{
	#if !defined(ESP32)
  		return;
	#endif

  memset(m_memory, 0, m_size);
}

template <typename T>
void SleepMemoryManager::ForEach(T &callable) const
{
	#if !defined(ESP32)
		return;
	#endif

  if (!m_memory)
    return;

  uint8_t *it = (uint8_t*)m_memory;
	uint8_t *itEnd = (uint8_t*)m_memory + m_size;

	Header const headerRef;
	
	Header *header = (Header*)it;
	
	if (header->m_idUint != headerRef.m_idUint)
	{
		return;
	}
	
	it += sizeof(Header);
	
	Chunk const chunkRef;
	
	for ( uint32_t i = 0 ; i < header->m_chunkCount ; ++i )
	{
		Chunk *chunk = (Chunk*)it;
		
		if (chunk->m_idUint != chunkRef.m_idUint)
		{
			LOG("SleepMemoryManager::Load ERROR:chunk id invalid:\n\r");
	
			break;
		}
		
		//LOG("SleepMemoryManager::Load chunk uid:0x%x\n\r", chunk->m_uid);
		
		it += sizeof(Chunk);
		
		bool const ret = callable(chunk->m_uid, it, chunk->m_size);

		if (!ret)
			return;
		
		it += chunk->m_allocatedSize;
	}
}

bool SleepMemoryManager::Load( uint32_t uid, Vector<uint8_t> &data ) const
{
	bool found = false;
	
	auto onChunk = [&](uint32_t chunkId, void const *chunkData, uint32_t chunkSize)
	{
		if (chunkId == uid)
		{
			found = true;
			data.resize(chunkSize);
			memcpy(data.data(), chunkData, chunkSize);
			return false;	//do not continue chunk parsing
		}

		return true;	//continue chunk parsing
	};

	ForEach(onChunk);

	return found;
}

bool SleepMemoryManager::Load( uint32_t uid, void *data, uint32_t expectedSize )
{
	bool found = false;
	auto onChunk = [&](uint32_t chunkId, void const *chunkData, uint32_t chunkSize)
	{
		if (chunkId == uid)
		{
			found = true;
			LOG("SleepMemoryManager::Load chunk uid 0x%x found @0x%x, copying  %d bytes\n\r", uid, (int32_t)chunkData, chunkSize);
		
			memcpy(data, chunkData, chunkSize);
			return false;	//do not continue chunk parsing
		}

		return true;	//continue chunk parsing
	};

	ForEach(onChunk);

	return found;
}

bool SleepMemoryManager::Write( void *dest, void const *data, uint32_t size )
{
	uint8_t *itEnd = (uint8_t*)m_memory + m_size;
	
	if ((uint8_t*)dest+size > itEnd)
	{
		return false;
	}
	
  //LOG("SleepMemoryManager::Write at:");LOG((int32_t)dest); LOG(" size:"); LOG(size); LOGLN("");
	
	memcpy(dest,data,size);
	
	return true;
}

bool SleepMemoryManager::Store( uint32_t uid, void const *data, uint32_t size )
{
	#if !defined(ESP32)
		return false;
	#endif

	if (!m_memory)
		return false;
	  
	uint8_t *it = (uint8_t*)m_memory;
	uint8_t *itEnd = (uint8_t*)m_memory + m_size;
	
	Header const headerRef;
	
	Header *header = (Header*)it;
	
	if (header->m_idUint != headerRef.m_idUint)
	{
		if ( !Write(header, &headerRef, sizeof(Header)) )
		{
			LOG("SleepMemoryManager::Store ERROR:header not written\n\r");
			
			return false;
		}
	}
	
	it += sizeof(Header);
	
	Chunk const chunkRef;
	for ( uint32_t i = 0 ; i < header->m_chunkCount ; ++i )
	{
		//LOG("SleepMemoryManager::Store chunk\n\r");
			
		Chunk *chunk = (Chunk*)it;
		
		if (chunk->m_idUint != chunkRef.m_idUint)
		{
			LOG("SleepMemoryManager::Store ERROR:chunk id invalid\n\r");
			header->m_chunkCount = i;
			break;
		}
		
		if (chunk->m_uid == uid)
		{
			LOG("SleepMemoryManager::Store ERROR:chunk with uid already exists\n\r");
			return false;
		}
		
		it += sizeof(Chunk);
		it += chunk->m_allocatedSize;

		//LOG("SleepMemoryManager::Store skip "); LOG(chunk->m_allocatedSize); LOGLN("");
	}
	
  
	Chunk &newChunk = *(Chunk*)it;
	
	Write(it, &chunkRef, sizeof(Chunk));
	
	newChunk.m_uid = uid;
	newChunk.m_size = size;
	newChunk.m_allocatedSize = (size + 15) & ~15;
	
	it += sizeof(Chunk);
	
	Write(it, data, size);
	
	header->m_chunkCount++;
	
	LOG_COND(IsVerbose(),"SleepMemoryManager::Stored new chunk uid:0x%x @0x%x size:%d(aligned:%d)\n\r", uid, (int32_t)it, size, newChunk.m_allocatedSize);
	
	return true;
}
