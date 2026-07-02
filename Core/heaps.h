//
// Waveson Password Generator
// Author: Stephen Higgins, https://github.com/viathefalcon
//

#pragma once

// Macros to allocate and release memory from the process heap
#define PH_ALLOC(Bytes) ::HeapAlloc( ::GetProcessHeap( ), HEAP_ZERO_MEMORY, (Bytes) )
#define PH_FREE(Buffer) ::HeapFree( ::GetProcessHeap( ), 0, (Buffer) )
