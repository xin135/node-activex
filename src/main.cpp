//-------------------------------------------------------------------------------------------------------
// Project: node-activex
// Author: Yuri Dursin
// Description: Defines the entry point for the NodeJS addon
//-------------------------------------------------------------------------------------------------------

#include "stdafx.h"
#include "disp.h"
#include "async.h"

job_processor_ptr job_processor;

//----------------------------------------------------------------------------------

namespace node_activex {

    void Init(Local<Object> exports) {
        DispObject::NodeInit(exports);
    }

    NODE_MODULE(node_activex, Init)
}

//----------------------------------------------------------------------------------

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ulReason, LPVOID lpReserved) {
    switch (ulReason) {
    case DLL_PROCESS_ATTACH:
        CoInitialize(0);
        break;
    case DLL_PROCESS_DETACH:
		if (job_processor) job_processor->stop();
		CoUninitialize();
        break;
    case DLL_THREAD_ATTACH:
        break;
    case DLL_THREAD_DETACH:
        break;
    }
    return TRUE;
}

//----------------------------------------------------------------------------------
