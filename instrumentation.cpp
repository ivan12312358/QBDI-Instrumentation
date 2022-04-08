#include <assert.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <QBDI.h>
#include <fstream>

static const size_t STACK_SIZE = 0x100000;

struct InstrData
{
	std::fstream log;
	QBDI::rword rbp{};
	QBDI::rword pnt{};
	size_t size{};

	InstrData(std::string filename = "log.txt"): 
		log(filename, log.trunc | log.out) {}
};

extern int* source(size_t);
extern void sink(int*);
extern void test();

QBDI::VMAction sourceInstrument(QBDI::VM *vm, QBDI::GPRState *gprState,
						  		QBDI::FPRState *fprState, void* data)
{
	InstrData* Data  = static_cast<InstrData*>(data);
	Data->rbp  = gprState->rbp;
	Data->size = gprState->rdi;

	return QBDI::VMAction::CONTINUE;
}

QBDI::VMAction sourceRetVal(QBDI::VM *vm, QBDI::GPRState *gprState,
							QBDI::FPRState *fprState, void* data)
{
	InstrData* Data = static_cast<InstrData*>(data);

	if (gprState->rbp == Data->rbp)
	{
		Data->pnt = gprState->rax;
		Data->rbp = 0;

		Data->log << "returned address from source(size_t): " 
				  << std::hex << Data->pnt  << " and size: "
				  << std::dec << Data->size << std::endl;
	}

	return QBDI::VMAction::CONTINUE;
}

QBDI::VMAction sinkInstrument(QBDI::VM *vm, QBDI::GPRState *gprState,
					  		  QBDI::FPRState *fprState, void* data)
{
	InstrData* Data = static_cast<InstrData*>(data);

	if (gprState->rdi - Data->pnt < Data->size * sizeof(int))
	{
		Data->log << "called sink(int*) with address: " 
				  << std::hex << gprState->rdi << std::endl;
	}

	return QBDI::VMAction::CONTINUE;
}

void setCBK(QBDI::VM *vm, InstrData *Data)
{
	uint32_t cid;
	
	cid = vm->addCodeAddrCB(reinterpret_cast<QBDI::rword>(source), QBDI::POSTINST, sourceInstrument, Data);
	assert(cid != QBDI::INVALID_EVENTID);
	cid = vm->addCodeAddrCB(reinterpret_cast<QBDI::rword>(sink),   QBDI::POSTINST, sinkInstrument,   Data);
	assert(cid != QBDI::INVALID_EVENTID);
	cid = vm->addMnemonicCB("ret*", QBDI::PREINST, sourceRetVal, Data);
	assert(cid != QBDI::INVALID_EVENTID);
}

int main()
{
	QBDI::VM vm{};
	QBDI::GPRState* state = vm.getGPRState();
	assert(state != nullptr);

	uint8_t *fakestack;
	bool res = QBDI::allocateVirtualStack(state, STACK_SIZE, &fakestack);
	assert(res == true);

	InstrData Data;
	setCBK(&vm, &Data);

	res = vm.addInstrumentedModuleFromAddr(reinterpret_cast<QBDI::rword>(test));
	assert(res == true);

	res = vm.call(nullptr, reinterpret_cast<QBDI::rword>(test));
	assert(res == true);

	QBDI::alignedFree(fakestack);

	return 0;
}