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

#include "Core/MIPS/IR.h"
#include "Core/MIPS/MIPSAnalyst.h"
#include "Core/MIPS/MIPSTables.h"
#include "Core/MemMap.h"
#include "Core/MIPS/MIPSDis.h"
#include "Core/MIPS/MIPSCodeUtils.h"
#include "Core/MIPS/JitCommon/JitCommon.h"
#include "Core/HLE/ReplaceTables.h"

namespace MIPSComp {

// Reorderings to do:
//   * Hoist loads and stores upwards as far as they will go
//   * Sort sequences of loads and stores by register to allow compiling into things like LDMIA
//   * Join together "pairs" like mfc0/mtv and mfv/mtc0
//   Returns true when it changed something. Might have to be called repeatedly to get everything done.
static bool Reorder(IRBlock *block);
static void ComputeLiveness(IRBlock *block);
static void DebugPrintBlock(IRBlock *block);

void Jit::ExtractIR(u32 address, IRBlock *block) {
	static int count = 0;
	count++;
	bool debugPrint = count < 10;
	if (debugPrint) {
		printf("======= %08x ========\n", address);
	}
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

	Reorder(block);
	ComputeLiveness(block);
	if (debugPrint) {
		DebugPrintBlock(block);
	}
}

static bool Reorder(IRBlock *block) {
	// Sweep downwards
	for (int i = 0; i < (int)block->entries.size() - 1; i++) {
		IREntry &e1 = block->entries[i];
		IREntry &e2 = block->entries[i + 1];
		// Do stuff!
	}
	// Then sweep upwards
	for (int i = (int)block->entries.size() - 1; i >= 0; i--) {
		IREntry &e1 = block->entries[i];
		IREntry &e2 = block->entries[i + 1];
		// Do stuff!
	}
	return false;
}

void ToBitString(char *ptr, u64 bits, int numBits) {
	for (int i = numBits - 1; i >= 0; i--) {
		*(ptr++) = (bits & (1ULL << i)) ? '1' : '.';
	}
	*ptr = '\0';
}

static void ComputeLiveness(IRBlock *block) {
	// Okay, now let's work backwards and compute liveness information.
	//
	// NOTE: This will not be accurate until all branch delay slots have been unfolded!

	// TODO: By following calling conventions etc, it may be possible to eliminate
	// additional register liveness from "jr ra" upwards. However, this is not guaranteed to work on all games.

	/*
	u64 gprLiveness = 0;
	u64 fprLiveness = 0;
	for (int i = 0; i < block->entries.size(); i++) {
		IREntry &e = block->entries[i];
		e.liveGPR = gprLiveness;
		e.liveFPR = fprLiveness;
	}*/

	// Hmm.. not sure this is right. Needs thinking.

	u64 gprLiveness = 0;  // note - nine Fs, for HI/LO/flags-in-registers. To define later.
	u32 fprLiveness = 0;
	for (int i = (int)block->entries.size() - 1; i >= 0; i--) {
		IREntry &e = block->entries[i];
		if (e.op == 0) { // nop
			continue;
		}
		if (e.info & IN_RS) gprLiveness |= (1ULL << MIPS_GET_RS(e.op));
		if (e.info & IN_RT) gprLiveness |= (1ULL << MIPS_GET_RT(e.op));
		if (e.info & IN_RT) gprLiveness |= (1ULL << MIPS_GET_RT(e.op));
		if (e.info & IN_LO) gprLiveness |= (1ULL << MIPS_REG_LO);
		if (e.info & IN_HI) gprLiveness |= (1ULL << MIPS_REG_HI);
		if (e.info & IN_FS) fprLiveness |= (1 << MIPS_GET_FS(e.op));
		if (e.info & IN_FT) fprLiveness |= (1 << MIPS_GET_FT(e.op));

		e.liveGPR = gprLiveness;
		e.liveFPR = fprLiveness;

		if (e.info & OUT_RT) gprLiveness &= ~(1ULL << MIPS_GET_RT(e.op));
		if (e.info & OUT_RD) gprLiveness &= ~(1ULL << MIPS_GET_RD(e.op));
		if (e.info & OUT_RA) gprLiveness &= ~(1ULL << MIPS_REG_RA);
		if (e.info & OUT_FD) fprLiveness &= ~(1 << MIPS_GET_FD(e.op));
		if (e.info & OUT_FS) fprLiveness &= ~(1 << MIPS_GET_FS(e.op));
		if (e.info & OUT_LO) gprLiveness &= ~(1ULL << MIPS_REG_LO);
		if (e.info & OUT_HI) gprLiveness &= ~(1ULL << MIPS_REG_HI);
	}
}

std::vector<std::string> IRBlock::ToStringVector() {
	std::vector<std::string> vec;
	char buf[512];
	for (int i = 0; i < (int)entries.size(); i++) {
		IREntry &e = entries[i];
		char instr[256], liveness1[40], liveness2[33];
		ToBitString(liveness1, e.liveGPR, 40);
		ToBitString(liveness2, e.liveFPR, 32);
		MIPSDisAsm(e.op, e.origAddress, instr, true);
		snprintf(buf, sizeof(buf), "%s : %s %s\n", instr, liveness1, liveness2);
		vec.push_back(std::string(buf));
	}
	return vec;
}

static void DebugPrintBlock(IRBlock *block) {
	std::vector<std::string> vec = block->ToStringVector();
	for (auto &s : vec) {
		printf("%s", s.c_str());
	}
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
