#include <assert.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include "instrumentation.h"



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

	res = vm.addInstrumentedModuleFromAddr(reinterpret_cast<QBDI::rword>(testSink));
	assert(res == true);

	res = vm.call(nullptr, reinterpret_cast<QBDI::rword>(testSink));
	assert(res == true);

	QBDI::alignedFree(fakestack);

	return 0;
}

QBDI::VMAction sourceInstrument(QBDI::VM *vm, QBDI::GPRState *gprState,
						  		QBDI::FPRState *fprState, void* data)
{
	InstrData* Data = static_cast<InstrData*>(data);
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
		Data->rbp = 0;
		Data->pnt = gprState->rax;
		Data->log << "returned address from source(size_t): "
				  << std::hex << Data->pnt << std::endl;

		for (size_t i = 0; i < Data->size; ++i)
			Data->instrData.insert(Data->pnt + i * sizeof(unsigned char));

		uint32_t cid;

		cid = vm->addMemAccessCB(QBDI::MEMORY_READ,  dataReadInstruction,  Data);
		assert(cid != QBDI::INVALID_EVENTID);
		cid = vm->addMemAccessCB(QBDI::MEMORY_WRITE, dataWriteInstruction, Data);
		assert(cid != QBDI::INVALID_EVENTID);
		cid = vm->addMnemonicCB("mov*", QBDI::PREINST, dataMoveInstruction, Data);
		// vm->addCodeCB(QBDI::PREINST, showInstruction, Data);
	}

	return QBDI::VMAction::CONTINUE;
}

QBDI::VMAction sinkInstrument(QBDI::VM *vm, QBDI::GPRState *gprState,
					  		  QBDI::FPRState *fprState, void* data)
{
	InstrData* Data = static_cast<InstrData*>(data);

	auto argIt = Data->instrData.find(QBDI::RDI_IDX);

	if (argIt != Data->instrData.cend())
	{
		Data->log << "called sink(int*) with value: " 
				  << gprState->rdi << std::endl;
	}

	return QBDI::VMAction::CONTINUE;
}

QBDI::VMAction dataReadInstruction(QBDI::VM *vm, QBDI::GPRState *gprState,
				   				   QBDI::FPRState *fprState, void* data)
{
	auto instAnalysis = vm->getInstAnalysis(QBDI::ANALYSIS_OPERANDS);

	std::vector<QBDI::MemoryAccess> mem = vm->getInstMemoryAccess();

	if (mem.empty() || (int) instAnalysis->numOperands < 6)
		return QBDI::VMAction::CONTINUE;

	QBDI::rword src = mem[0].accessAddress;
	QBDI::rword	dst = instAnalysis->operands[0].regCtxIdx;

	InstrData* Data = static_cast<InstrData*>(data);
	Data->controlMove(src, dst);

	return QBDI::VMAction::CONTINUE;
}

QBDI::VMAction dataWriteInstruction(QBDI::VM *vm, QBDI::GPRState *gprState,
				   				    QBDI::FPRState *fprState, void* data)
{
	auto instAnalysis = vm->getInstAnalysis(QBDI::ANALYSIS_OPERANDS);

	std::vector<QBDI::MemoryAccess> mem = vm->getInstMemoryAccess();

	if (mem.empty() || (int) instAnalysis->numOperands < 6)
		return QBDI::VMAction::CONTINUE;

	QBDI::rword dst = mem[0].accessAddress;
	QBDI::rword	src = instAnalysis->operands[5].regCtxIdx;

	InstrData* Data = static_cast<InstrData*>(data);
	Data->controlMove(src, dst);

	return QBDI::VMAction::CONTINUE;
}

QBDI::VMAction dataMoveInstruction(QBDI::VM *vm, QBDI::GPRState *gprState,
				   				   QBDI::FPRState *fprState, void* data)
{
	auto instAnalysis = vm->getInstAnalysis(QBDI::ANALYSIS_OPERANDS);

	if ((int)instAnalysis->numOperands != 2)
		return QBDI::VMAction::CONTINUE;
	else if (instAnalysis->operands[1].type != QBDI::OPERAND_GPR)
		return QBDI::VMAction::CONTINUE;

	QBDI::rword dst = instAnalysis->operands[0].regCtxIdx,
				src = instAnalysis->operands[1].regCtxIdx;

	InstrData* Data = static_cast<InstrData*>(data);
	Data->controlMove(src, dst);

	return QBDI::VMAction::CONTINUE;
}

QBDI::VMAction dataAndInstruction(QBDI::VM *vm, QBDI::GPRState *gprState,
				   				  QBDI::FPRState *fprState, void* data)
{
	auto instAnalysis = vm->getInstAnalysis(QBDI::ANALYSIS_OPERANDS);

	QBDI::rword dst = instAnalysis->operands[0].regCtxIdx;

	InstrData* Data = static_cast<InstrData*>(data);
	auto dstIt = Data->instrData.find(dst);

	if (dstIt != Data->instrData.cend())
	{
		auto srcOp = instAnalysis->operands[1];

		QBDI::rword src	= 0;

		if (srcOp.type == QBDI::OPERAND_GPR)
			src = *((QBDI::rword*)gprState + srcOp.regCtxIdx);
		else if (srcOp.type == QBDI::OPERAND_IMM)
			src = srcOp.value;
		else
			return QBDI::VMAction::CONTINUE;
	
		if (src == 0x0)
			Data->instrData.erase(dstIt);
	}

	return QBDI::VMAction::CONTINUE;
}

void setCBK(QBDI::VM *vm, InstrData *Data)
{
	uint32_t cid;

	cid = vm->addCodeAddrCB(reinterpret_cast<QBDI::rword>(source), QBDI::POSTINST, sourceInstrument, Data);
	assert(cid != QBDI::INVALID_EVENTID);
	cid = vm->addCodeAddrCB(reinterpret_cast<QBDI::rword>(sink), QBDI::POSTINST, sinkInstrument, Data);
	assert(cid != QBDI::INVALID_EVENTID);
	cid = vm->addMnemonicCB("ret*", QBDI::PREINST, sourceRetVal, Data);
	assert(cid != QBDI::INVALID_EVENTID);
	cid = vm->addMnemonicCB("and*", QBDI::PREINST, dataAndInstruction, Data);
	assert(cid != QBDI::INVALID_EVENTID);
}
