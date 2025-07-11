#include <dolphin.h>
#include <dolphin/ar.h>
#include <macros.h>

#include "ar/__ar.h"

static struct ARQRequest* __ARQRequestQueueHi;
static struct ARQRequest* __ARQRequestTailHi;
static struct ARQRequest* __ARQRequestQueueLo;
static struct ARQRequest* __ARQRequestTailLo;
static struct ARQRequest* __ARQRequestPendingHi;
static struct ARQRequest* __ARQRequestPendingLo;
static ARQCallback __ARQCallbackHi;
static ARQCallback __ARQCallbackLo;
static u32 __ARQChunkSize;
static int __ARQ_init_flag;

inline void __ARQPopTaskQueueHi(void)
{
	if (__ARQRequestQueueHi) {
		if (__ARQRequestQueueHi->type == 0) {
			ARStartDMA(__ARQRequestQueueHi->type, __ARQRequestQueueHi->source,
			           __ARQRequestQueueHi->dest, __ARQRequestQueueHi->length);
		} else {
			ARStartDMA(__ARQRequestQueueHi->type, __ARQRequestQueueHi->dest,
			           __ARQRequestQueueHi->source,
			           __ARQRequestQueueHi->length);
		}
		__ARQCallbackHi       = __ARQRequestQueueHi->callback;
		__ARQRequestPendingHi = __ARQRequestQueueHi;
		__ARQRequestQueueHi   = __ARQRequestQueueHi->next;
	}
}

void __ARQServiceQueueLo(void)
{
	if (__ARQRequestPendingLo == 0 && __ARQRequestQueueLo) {
		__ARQRequestPendingLo = __ARQRequestQueueLo;
		__ARQRequestQueueLo   = __ARQRequestQueueLo->next;
	}
	if (__ARQRequestPendingLo) {
		if (__ARQRequestPendingLo->length <= __ARQChunkSize) {
			if (__ARQRequestPendingLo->type == 0) {
				ARStartDMA(
				    __ARQRequestPendingLo->type, __ARQRequestPendingLo->source,
				    __ARQRequestPendingLo->dest, __ARQRequestPendingLo->length);
			} else {
				ARStartDMA(__ARQRequestPendingLo->type,
				           __ARQRequestPendingLo->dest,
				           __ARQRequestPendingLo->source,
				           __ARQRequestPendingLo->length);
			}
			__ARQCallbackLo = __ARQRequestPendingLo->callback;
		} else if (__ARQRequestPendingLo->type == 0) {
			ARStartDMA(__ARQRequestPendingLo->type,
			           __ARQRequestPendingLo->source,
			           __ARQRequestPendingLo->dest, __ARQChunkSize);
		} else {
			ARStartDMA(__ARQRequestPendingLo->type, __ARQRequestPendingLo->dest,
			           __ARQRequestPendingLo->source, __ARQChunkSize);
		}
		__ARQRequestPendingLo->length -= __ARQChunkSize;
		__ARQRequestPendingLo->source += __ARQChunkSize;
		__ARQRequestPendingLo->dest += __ARQChunkSize;
	}
}

void __ARQCallbackHack(u32 unused) { }

void __ARQInterruptServiceRoutine()
{
	if (__ARQCallbackHi) {
		__ARQCallbackHi((u32)__ARQRequestPendingHi);
		__ARQRequestPendingHi = NULL;
		__ARQCallbackHi       = NULL;
	} else if (__ARQCallbackLo) {
		__ARQCallbackLo((u32)__ARQRequestPendingLo);
		__ARQRequestPendingLo = NULL;
		__ARQCallbackLo       = NULL;
	}
	__ARQPopTaskQueueHi();
	if (__ARQRequestPendingHi == 0) {
		__ARQServiceQueueLo();
	}
}

void ARQInit(void)
{
	if (__ARQ_init_flag != 1) {
		__ARQRequestQueueHi = __ARQRequestQueueLo = NULL;
		__ARQChunkSize                            = 0x1000;
		ARRegisterDMACallback(__ARQInterruptServiceRoutine);
		__ARQRequestPendingHi = NULL;
		__ARQRequestPendingLo = NULL;
		__ARQCallbackHi       = NULL;
		__ARQCallbackLo       = NULL;
		__ARQ_init_flag       = 1;
	}
}

void ARQPostRequest(struct ARQRequest* request, u32 owner, u32 type,
                    u32 priority, u32 source, u32 dest, u32 length,
                    ARQCallback callback)
{
	int level;

	ASSERTLINE(0x1A9, request);
	ASSERTLINE(0x1AA, (type == ARQ_TYPE_MRAM_TO_ARAM)
	                      || (type == ARQ_TYPE_ARAM_TO_MRAM));
	ASSERTLINE(0x1AB, (priority == ARQ_PRIORITY_LOW)
	                      || (priority == ARQ_PRIORITY_HIGH));
	ASSERTLINE(0x1AE, (length % ARQ_DMA_ALIGNMENT) == 0);
	request->next   = NULL;
	request->owner  = owner;
	request->type   = type;
	request->source = source;
	request->dest   = dest;
	request->length = length;
	if (callback) {
		request->callback = callback;
	} else {
		request->callback = __ARQCallbackHack;
	}
	level = OSDisableInterrupts();
	switch (priority) {
	case ARQ_PRIORITY_LOW:
		if (__ARQRequestQueueLo) {
			__ARQRequestTailLo->next = request;
		} else {
			__ARQRequestQueueLo = request;
		}
		__ARQRequestTailLo = request;
		break;
	case ARQ_PRIORITY_HIGH:
		if (__ARQRequestQueueHi) {
			__ARQRequestTailHi->next = request;
		} else {
			__ARQRequestQueueHi = request;
		}
		__ARQRequestTailHi = request;
		break;
	}
	if ((__ARQRequestPendingHi == 0) && (__ARQRequestPendingLo == 0)) {
		__ARQPopTaskQueueHi();
		if (__ARQRequestPendingHi == 0) {
			__ARQServiceQueueLo();
		}
	}
	OSRestoreInterrupts(level);
}
