#include "bd.h"
#include <fstream>
#include <cstring>
#include <string>

static const double F0[] = {0.0, 0.9375, 1.796875, 1.53125, 1.90625};
static const double F1[] = {0.0, 0.0, -0.8125, -0.859375, -0.9375};

bool BDParser::load(const QString& path) {
    LogInfo("Loading BD: " + path.toStdString());

    std::ifstream file(path.toStdString(), std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        LogErr("Could not open BD file.");
        return false;
    }

    size_t size = file.tellg();
    if (size == 0) {
        LogErr("BD file is empty.");
        return false;
    }

    data.resize(size);
    file.seekg(0);
    file.read(reinterpret_cast<char*>(data.data()), data.size());

    LogInfo("Loaded " + std::to_string(size) + " bytes.");
    return true;
}

std::vector<u8> BDParser::get_adpcm_block(u32 start_offset) {
    if (start_offset >= data.size()) {
        LogErr("Offset out of bounds: " + std::to_string(start_offset));
        return {};
    }

    std::vector<u8> raw_blocks;
    size_t cursor = start_offset;

    while (true) {
        if (cursor + 16 > data.size()) break;

        size_t current_size = raw_blocks.size();
        raw_blocks.resize(current_size + 16);
        std::memcpy(raw_blocks.data() + current_size, data.data() + cursor, 16);

        u8 flags = data[cursor + 1];

        // Silence Loop Hack
        bool isSilenceEnd = false;
        if (cursor + 16 <= data.size()) {
            if (data[cursor] == 0x00 && data[cursor+1] == 0x07 && data[cursor+2] == 0x77) {
                isSilenceEnd = true;
            }
        }

        cursor += 16;
        if ((flags & 1) || isSilenceEnd) break;
        if (raw_blocks.size() > 4 * 1024 * 1024) break;
    }
    return raw_blocks;
}

DecodedSample BDParser::decode_adpcm(const std::vector<u8>& adpcm_data, u32 sample_rate) {
    DecodedSample result;
    result.sample_rate = sample_rate;

    if (adpcm_data.empty()) return result;

    std::vector<s16> samples;
    double s1 = 0, s2 = 0;
    int num_blocks = adpcm_data.size() / 16;
    samples.reserve(num_blocks * 28);

    for (int b = 0; b < num_blocks; b++) {
        int offset = b * 16;
        u8 shift_filter = adpcm_data[offset];
        u8 flags = adpcm_data[offset+1];

        int shift = 12 - (shift_filter & 0x0F);
        int filter_idx = (shift_filter >> 4) & 0x07;
        if (filter_idx > 4) filter_idx = 0;

        bool isSilenceHack = (shift_filter == 0x00 && flags == 0x07 && adpcm_data[offset+2] == 0x77);

        if ((flags & 4)) result.loop_start = (u32)samples.size();
        if ((flags & 2)) result.looping = true;
        if (((flags & 1) || isSilenceHack) && result.looping) {
            result.loop_end = (u32)samples.size() + 28;
        }

        for (int i = 2; i < 16; i++) {
            u8 byte = adpcm_data[offset + i];
            int nibbles[2] = { byte & 0x0F, (byte >> 4) & 0x0F };

            for (int nib : nibbles) {
                int val_s = (nib < 8) ? nib : nib - 16;
                double sample;
                if (shift >= 0) sample = (double)(val_s << shift);
                else sample = (double)(val_s >> (-shift));

                double val = sample + (s1 * F0[filter_idx]) + (s2 * F1[filter_idx]);
                s2 = s1; s1 = val;

                if (val > 32767.0) val = 32767.0;
                if (val < -32768.0) val = -32768.0;
                samples.push_back((s16)val);
            }
        }
    }

    if (result.loop_end == 0) result.loop_end = (u32)samples.size();
    result.pcm = samples;
    return result;
}
