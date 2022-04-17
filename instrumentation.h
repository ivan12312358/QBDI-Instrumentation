#include <fstream>
#include <unordered_set>
#include <QBDI.h>

static const size_t STACK_SIZE = 0x100000;

namespace QBDI
{
	static const rword RDI_IDX = 5;
}

struct InstrData
{
	using instrDataIt = std::unordered_set<QBDI::rword>::iterator;

    std::unordered_set<QBDI::rword> instrData;
	std::fstream log;
	QBDI::rword rbp{};
	QBDI::rword pnt{};
    size_t size{};

	InstrData(std::string filename = "log.txt"): 
		log(filename, log.trunc | log.out), instrData() {}

	void controlMove(QBDI::rword src, QBDI::rword dst)
	{
		auto srcIt = instrData.find(src),
			 dstIt = instrData.find(dst),
			 endIt = instrData.end();

		if (dstIt == endIt && srcIt != endIt)
			instrData.insert(dst);
		else if (dstIt != endIt && srcIt == endIt)
		{
			if (dst - pnt >= size * sizeof (unsigned char))
				instrData.erase(dstIt);
		}
	}
};

extern unsigned char* source(size_t);
extern unsigned char* sink(unsigned char);
extern int testSink();

QBDI::VMAction sourceInstrument(QBDI::VM *vm, QBDI::GPRState *gprState,
						  		QBDI::FPRState *fprState, void* data);

QBDI::VMAction sourceRetVal(QBDI::VM *vm, QBDI::GPRState *gprState,
							QBDI::FPRState *fprState, void* data);

QBDI::VMAction dataWriteInstruction(QBDI::VM *vm, QBDI::GPRState *gprState,
				   				    QBDI::FPRState *fprState, void* data);

QBDI::VMAction dataReadInstruction(QBDI::VM *vm, QBDI::GPRState *gprState,
				   				   QBDI::FPRState *fprState, void* data);

QBDI::VMAction dataMoveInstruction(QBDI::VM *vm, QBDI::GPRState *gprState,
				   				   QBDI::FPRState *fprState, void* data);

QBDI::VMAction dataAndInstruction(QBDI::VM *vm, QBDI::GPRState *gprState,
				   				  QBDI::FPRState *fprState, void* data);

void setCBK(QBDI::VM *vm, InstrData *Data);
