#include "hd.h"
#include <fstream>
#include <algorithm>
#include <cstring>

#define null 0xFFFFFFFF

/*TODO: do something for those checks */

bool HDParser::load(const QString& path, Bank& bank) {
    LogInfo("Loading HD: " + path.toStdString());

    std::ifstream file(path.toStdString(), std::ios::binary);
    if (!file.is_open()) {
        LogErr("Could not open file.");
        return false;
    }

    auto readAt = [&](u32 offset, void* dest, size_t size) -> bool {
        if (offset == null) return false;

        file.clear();
        file.seekg(offset, std::ios::beg);
        if (file.fail()) return false;

        file.read(reinterpret_cast<char*>(dest), size);
        return (file.gcount() == (std::streamsize)size);
    };

    auto convertPanValue = [](u8 panVal) -> int {
        if (panVal > 0x7F) return (int)0x40 - (int)(panVal - 0x7F);
        return (int)panVal - 0x40;
    };

    VersCk vers;
    if (!readAt(0, &vers, sizeof(VersCk))) return false;

    if (vers.Creator != 0x53434549) {
        LogErr("Invalid IECS Magic");
        return false;
    }

    HdrCk hdr;
    u32 hdrOffset = (vers.chunkSize < 16) ? 16 : vers.chunkSize;
    if (!readAt(hdrOffset, &hdr, sizeof(HdrCk))) return false;

    auto loadOffsets = [&](u32 chunkAddr) -> std::vector<u32> {
        if (chunkAddr == 0 || chunkAddr == null) return {};

        u32 count = 0;
        if (!readAt(chunkAddr + 12, &count, 4)) return {};
        if (count > 100000) return {};

        std::vector<u32> offsets(count + 1);
        if (!readAt(chunkAddr + 16, offsets.data(), (count + 1) * 4)) return {};

        for (auto& off : offsets) {
            if (off != null) off += chunkAddr;
        }
        return offsets;
    };

    auto vagOffsets = loadOffsets(hdr.vagInfoChunkAddr);
    auto sampOffsets = loadOffsets(hdr.sampleChunkAddr);
    auto setOffsets = loadOffsets(hdr.samplesetChunkAddr);
    auto progOffsets = loadOffsets(hdr.programChunkAddr);

    std::vector<VAGInfoParam> vags(vagOffsets.size());
    for(size_t i=0; i<vagOffsets.size(); ++i) readAt(vagOffsets[i], &vags[i], sizeof(VAGInfoParam));

    std::vector<SampleParam> samps(sampOffsets.size());
    for(size_t i=0; i<sampOffsets.size(); ++i) readAt(sampOffsets[i], &samps[i], sizeof(SampleParam));

    LogInfo("Loaded Tables: " + std::to_string(progOffsets.size()) + " Programs, " +
    std::to_string(samps.size()) + " Samples.");

    bank.programs.clear();

    for (u32 i = 0; i < progOffsets.size(); ++i) {
        if (progOffsets[i] == null) continue;

        auto prog = std::make_shared<Program>();
        prog->id = i;
        prog->name = "Program " + std::to_string(i);

        ProgParam pp;
        if (!readAt(progOffsets[i], &pp, sizeof(ProgParam))) continue;

        prog->master_vol = pp.progVolume;
        prog->master_pan = pp.progPanpot;

        u32 splitBase = progOffsets[i] + pp.splitBlockAddr;
        if (pp.nSplit > 128) pp.nSplit = 0;

        for (int s = 0; s < pp.nSplit; ++s) {
            SplitBlock sb;
            if (!readAt(splitBase + (s * sizeof(SplitBlock)), &sb, sizeof(SplitBlock))) break;

            if (sb.sampleSetIndex >= setOffsets.size()) continue;
            u32 setAddr = setOffsets[sb.sampleSetIndex];
            if (setAddr == null) continue;

            u8 nSamples;
            if (!readAt(setAddr + 3, &nSamples, 1)) continue;
            if (nSamples > 16) nSamples = 16;

            std::vector<u16> sIndices(nSamples);
            if (!readAt(setAddr + 4, sIndices.data(), nSamples * 2)) continue;

            for (u16 sIdx : sIndices) {
                if (sIdx >= samps.size() || sampOffsets[sIdx] == null) continue;
                const auto& sp = samps[sIdx];

                if (sp.VagIndex >= vags.size() || vagOffsets[sp.VagIndex] == null) continue;
                const auto& vp = vags[sp.VagIndex];

                Tone t;
                t.min_note = sb.splitRangeLow;
                t.max_note = (sb.splitRangeHigh < sb.splitRangeLow) ? 0x7F : sb.splitRangeHigh;
                t.root_key = sp.sampleBaseNote;
                t.pitch_fine = sb.splitDetune + sp.sampleDetune;

                int finalPan = 0x40 + convertPanValue(sp.samplePanpot) + convertPanValue(pp.progPanpot) + convertPanValue(sb.splitPanpot);
                t.pan = std::clamp(finalPan, 0, 127);

                t.volume = sp.sampleVolume;
                t.adsr1 = sp.sampleAdsr1;
                t.adsr2 = sp.sampleAdsr2;
                t.bd_offset = vp.vagOffsetAddr;
                t.sample_rate = vp.vagSampleRate;
                t.is_reverb_enabled = (sp.sampleGroup & 0x4) || (sp.sampleGroup & 0x8);

                prog->tones.push_back(t);
            }
        }

        prog->is_layered = (prog->tones.size() > 1);
        bank.programs.push_back(prog);
    }

    bank.valid = true;
    return true;
}
