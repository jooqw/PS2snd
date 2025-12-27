#ifndef BD_H
#define BD_H

#include "main.h"
#include <QString>
#include <vector>

struct DecodedSample {
    std::vector<s16> pcm;
    u32 loop_start = 0;
    u32 loop_end = 0;
    bool looping = false;
    u32 sample_rate = 44100;
};

class BDParser {
public:
    bool load(const QString& path);
    std::vector<u8> get_adpcm_block(u32 start_offset);
    static DecodedSample decode_adpcm(const std::vector<u8>& adpcm_data, u32 sample_rate);

private:
    std::vector<u8> data;
};

#endif // BD_H
