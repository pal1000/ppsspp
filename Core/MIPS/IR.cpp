// Copyright (c) 2014- PPSSPP Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later versions.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official git repository and contact information can be found at
// https://github.com/hrydgard/ppsspp and http://www.ppsspp.org/.

#ifdef _WIN32
#include <Windows.h>
#endif

#include "Core/MIPS/IR.h"
#include "Core/MIPS/MIPSAnalyst.h"
#include "Core/MIPS/MIPSTables.h"
#include "Core/MemMap.h"
#include "Core/MIPS/MIPSDis.h"
#include "Core/MIPS/MIPSCodeUtils.h"
#include "Core/MIPS/JitCommon/JitCommon.h"
#include "Core/HLE/ReplaceTables.h"

namespace MIPSComp {

// Not very elegant..
//#ifdef ARM
//void ArmJit::ExtractIR(u32 address, IRBlock *block) {
//#else
void Jit::ExtractIR(u32 address, IRBlock *block) {
//#endif
	block->entries.clear();

	block->analysis = MIPSAnalyst::Analyze(address);

	// TODO: This loop could easily follow branches and whatnot, perform inlining and so on.

	int exitInInstructions = -1;
	while (true) {
		IREntry e;
		MIPSOpcode op = Memory::Read_Opcode_JIT(address);
		e.origAddress = address;
		e.op = op;
		e.info = MIPSGetInfo(op);
		e.flags = 0;
		block->entries.push_back(e);

		if (e.info & DELAYSLOT) {
			// Check if replaceable JAL. If not, bail in 2 instructions.
			bool replacableJal = false;
			if (e.info & IS_JUMP) {
				if ((e.op >> 26) == 3) {
					// Definitely a JAL
					const ReplacementTableEntry *entry;
					if (CanReplaceJalTo(MIPSCodeUtils::GetJumpTarget(e.origAddress), &entry))
						replacableJal = true;
				}
			}

			if (!replacableJal) {
				exitInInstructions = 2;
			}
		}

		address += 4;

		if (exitInInstructions > 0)
			exitInInstructions--;
		if (exitInInstructions == 0)
			break;
	}

	// Okay, now let's work backwards and compute liveness information.

	// TODO: By following calling conventions etc, it may be possible to eliminate
	// additional register liveness from "jr ra" upwards. However, this is not guaranteed to work on all games.

	u64 gprLiveness = 0xFFFFFFFFFULL;  // note - nine Fs, for HI/LO/flags-in-registers. To define later.
	u32 fprLiveness = 0xFFFFFFFF;
	for (int i = (int)block->entries.size() - 1; i >= 0; i--) {
		IREntry &e = block->entries[i];
		e.liveGPR = gprLiveness;
		e.liveFPR = fprLiveness;

		//if (e.info & OUT_RT) {
		//	int rt = 
		//}
	}

	/*
	for (int i = 0; i < (int)block->entries.size(); i++) {
		IREntry &e = block->entries[i];
		char buffer[256];
		MIPSDisAsm(e.op, e.origAddress, buffer, true);
		OutputDebugStringA(buffer);
		OutputDebugStringA("\n");
	}
	OutputDebugStringA("===\n");
	*/
}

bool IRBlock::IsRegisterUsed(MIPSGPReg reg, int pos, int instrs) {
	int end = pos + instrs;
	while (pos < end) {
		MIPSOpcode op = entries[pos].op;
		const MIPSInfo info = entries[pos].info;

		// Yes, used.
		if ((info & IN_RS) && (MIPS_GET_RS(op) == reg))
			return true;
		if ((info & IN_RT) && (MIPS_GET_RT(op) == reg))
			return true;

		// Clobbered, so not used.
		if ((info & OUT_RT) && (MIPS_GET_RT(op) == reg))
			return false;
		if ((info & OUT_RD) && (MIPS_GET_RD(op) == reg))
			return false;
		if ((info & OUT_RA) && (reg == MIPS_REG_RA))
			return false;

		// Bail early if we hit a branch (could follow each path for continuing?)
		if ((info & IS_CONDBRANCH) || (info & IS_JUMP)) {
			// Still need to check the delay slot (so end after it.)
			// We'll assume likely are taken.
			end = pos + 2;
		}
		pos++;
	}
	return false;
}

}
