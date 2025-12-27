#ifndef HD_H
#define HD_H

#include "main.h"
#include <QString>
#include <vector>
#include <memory>
#include <string>

struct Tone {
    u8 min_note; u8 max_note;
    u8 root_key; s8 pitch_fine;
    u8 pan; u8 volume;
    u16 adsr1; u16 adsr2;
    u32 bd_offset;
    u32 sample_rate;
    bool is_reverb_enabled;
};

struct Program {
    u32 id;
    std::string name;
    std::vector<Tone> tones;
    u8 master_vol;
    u8 master_pan;
    bool is_layered;
};

struct Bank {
    std::vector<std::shared_ptr<Program>> programs;
    bool valid = false;
};


// ==========================================================

#pragma pack(push, 1)

struct VersCk {
    u32 Creator; u32 Type; u32 chunkSize;
    u16 reserved; u8 major; u8 minor;
};

struct HdrCk {
    u32 Creator; u32 Type; u32 chunkSize;
    u32 fileSize; u32 bodySize;
    u32 programChunkAddr; u32 samplesetChunkAddr; u32 sampleChunkAddr;
    u32 vagInfoChunkAddr; u32 seTimbreChunkAddr; u32 reserved[8];
};

struct ProgParam {
    u32 splitBlockAddr;
    u8 nSplit; u8 sizeSplitBlock;
    u8 progVolume; u8 progPanpot; s8 progTranspose; s8 progDetune; // progPanpot -> u8
    s8 keyFollowPan; u8 keyFollowPanCenter; u8 progAttr; u8 reserved;
};

struct SplitBlock {
    u16 sampleSetIndex;
    u8 splitRangeLow; u8 splitCrossFade; u8 splitRangeHigh; u8 splitNumber;
    u16 splitBendRangeLow; u16 splitBendRangeHigh;
    s8 kfPitch; u8 kfPitchCenter; s8 kfAmp; u8 kfAmpCenter;
    s8 kfPan; u8 kfPanCenter;
    u8 splitVolume; u8 splitPanpot; s8 splitTranspose; s8 splitDetune; // splitPanpot -> u8
};

struct SampleParam {
    u16 VagIndex;
    u8 velRangeLow; u8 velCrossFade; u8 velRangeHigh;
    s8 velFollowPitch; u8 velFollowPitchCenter; u8 velFollowPitchCurve;
    s8 velFollowAmp; u8 velFollowAmpCenter; u8 velFollowAmpCurve;
    u8 sampleBaseNote; s8 sampleDetune; u8 samplePanpot; // samplePanpot -> u8
    u8 sampleGroup; u8 samplePriority; u8 sampleVolume; u8 reserved;
    u16 sampleAdsr1; u16 sampleAdsr2;
};

struct VAGInfoParam {
    u32 vagOffsetAddr;
    u16 vagSampleRate;
    u8 vagAttribute; u8 reserved;
};

#pragma pack(pop)

class HDParser {
public:
    bool load(const QString& path, Bank& bank);
};

#endif // HD_H
