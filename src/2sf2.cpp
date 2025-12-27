#include "2sf2.h"
#include <sf2cute.hpp>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <map>
#include <iostream>
#include <vector>
#include <memory>

using namespace sf2cute;

// Kill me. ported from apeplayer

static inline uint32_t get_bits(uint32_t val, int start, int len) {
    return (val >> start) & ((1 << len) - 1);
}

struct VolumeEnvelope {
    u8 rate;
    bool decreasing;
    bool exponential;
    bool phase_invert;
    s32 counter;
    s32 counter_increment;
    s32 step;

    void Reset(u8 rate_, u8 rate_mask_, bool decreasing_, bool exponential_, bool phase_invert_);
    bool Tick(s16& current_level);
};

class HardwareADSR {
public:
    enum class Phase { Attack, Decay, Sustain, Release, Off };

    u32 reg_val;
    Phase phase;
    s16 current_volume;
    s16 target_volume;
    VolumeEnvelope envelope;

    HardwareADSR(u32 val) : reg_val(val), phase(Phase::Off), current_volume(0), target_volume(0) {}

    void KeyOn();
    void KeyOff();
    void UpdateEnvelope();
    s16 Tick();

    // Static simulator for SF2 timecent conversion
    static int16_t simulate_timecents(u32 reg_val, Phase target_phase);
};

// --- VolumeEnvelope Implementation ---

void VolumeEnvelope::Reset(u8 rate_, u8 rate_mask_, bool decreasing_, bool exponential_, bool phase_invert_) {
    rate = rate_;
    decreasing = decreasing_;
    exponential = exponential_;
    phase_invert = phase_invert_ && !(decreasing_ && exponential_);
    counter = 0;
    counter_increment = 0x8000;

    const s16 base_step = 7 - (rate & 3);
    step = ((decreasing_ ^ phase_invert_) | (decreasing_ & exponential_)) ? ~base_step : base_step;

    if (rate < 44) {
        step <<= (11 - (rate >> 2));
    }
    else if (rate >= 48) {
        counter_increment >>= ((rate >> 2) - 11);
        if ((rate & rate_mask_) != rate_mask_)
            counter_increment = std::max<u16>(counter_increment, 1u);
    }
}

bool VolumeEnvelope::Tick(s16& current_level) {
    u32 this_increment = counter_increment;
    s32 this_step = step;

    if (exponential) {
        if (decreasing) this_step = (this_step * current_level) >> 15;
        else {
            if (current_level >= 0x6000) {
                if (rate < 40) this_step >>= 2;
                else if (rate >= 44) this_increment >>= 2;
                else { this_step >>= 1; this_increment >>= 1; }
            }
        }
    }

    counter += this_increment;
    if (!(counter & 0x8000)) return true;

    counter = 0;
    s32 new_level = current_level + this_step;

    if (!decreasing) {
        if (new_level < -32768) new_level = -32768;
        if (new_level > 32767) new_level = 32767;
        current_level = (s16)new_level;
        return (new_level != ((this_step < 0) ? -32768 : 32767));
    } else {
        if (phase_invert) {
            if (new_level < -32768) new_level = -32768;
            if (new_level > 0) new_level = 0;
        }
        else {
            if (new_level < 0) new_level = 0;
        }
        current_level = (s16)new_level;
        return (new_level == 0);
    }
}

void HardwareADSR::KeyOn() {
    current_volume = 0;
    phase = Phase::Attack;
    UpdateEnvelope();
}

void HardwareADSR::KeyOff() {
    if (phase == Phase::Off || phase == Phase::Release) return;
    phase = Phase::Release;
    UpdateEnvelope();
}

void HardwareADSR::UpdateEnvelope() {
    u32 sustain_level = get_bits(reg_val, 0, 4);
    u32 decay_shift = get_bits(reg_val, 4, 4);
    u32 attack_step = get_bits(reg_val, 8, 2);
    u32 attack_shift = get_bits(reg_val, 10, 5);
    bool attack_exp = get_bits(reg_val, 15, 1);
    u32 release_shift = get_bits(reg_val, 16, 5);
    bool release_exp = get_bits(reg_val, 21, 1);
    u32 sustain_step = get_bits(reg_val, 22, 2);
    u32 sustain_shift = get_bits(reg_val, 24, 5);
    bool sustain_dec = get_bits(reg_val, 30, 1);
    bool sustain_exp = get_bits(reg_val, 31, 1);

    u8 attack_rate = (attack_shift << 2) | attack_step;
    u8 decay_rate = (decay_shift << 2);
    u8 sustain_rate = (sustain_shift << 2) | sustain_step;
    u8 release_rate = (release_shift << 2);

    switch(phase) {
        case Phase::Off:
            target_volume = 0;
            envelope.Reset(0, 0, false, false, false);
            break;
        case Phase::Attack:
            target_volume = 32767;
            envelope.Reset(attack_rate, 0x7F, false, attack_exp, false);
            break;
        case Phase::Decay:
            target_volume = (s16)std::min<s32>((sustain_level + 1) * 0x800, 32767);
            envelope.Reset(decay_rate, 0x1F << 2, true, true, false);
            break;
        case Phase::Sustain:
            target_volume = 0;
            envelope.Reset(sustain_rate, 0x7F, sustain_dec, sustain_exp, false);
            break;
        case Phase::Release:
            target_volume = 0;
            envelope.Reset(release_rate, 0x1F << 2, true, release_exp, false);
            break;
    }
}

s16 HardwareADSR::Tick() {
    if (phase == Phase::Off) return 0;

    if (envelope.counter_increment > 0)
        envelope.Tick(current_volume);

    if (phase != Phase::Sustain) {
        bool reached = envelope.decreasing ? (current_volume <= target_volume) : (current_volume >= target_volume);
        if (reached) {
            if (phase == Phase::Attack) phase = Phase::Decay;
            else if (phase == Phase::Decay) phase = Phase::Sustain;
            else if (phase == Phase::Release) phase = Phase::Off;
            UpdateEnvelope();
        }
    }
    return current_volume;
}

int16_t HardwareADSR::simulate_timecents(u32 reg_val, Phase target_phase) {
    HardwareADSR sim(reg_val);
    sim.phase = target_phase;
    sim.current_volume = (target_phase == Phase::Attack) ? 0 : 32767;
    sim.UpdateEnvelope();

    if (sim.envelope.counter_increment == 0) return -32768; // Instant/Zero duration

    int samples = 0;
    int limit = 44100 * 15;

    while (samples < limit) {
        sim.envelope.Tick(sim.current_volume);

        bool finished = false;
        if (target_phase == Phase::Attack) {
            finished = (sim.current_volume >= 32767);
        } else {
            // For Decay/Release/Sustain
            if (target_phase == Phase::Decay) {
                // Decay finishes when it hits sustain level
                finished = (sim.current_volume <= sim.target_volume);
            } else {
                finished = (sim.current_volume <= 0);
            }
        }

        if (finished) break;
        samples++;
    }

    if (samples <= 1) return -32768;

    double seconds = (double)samples / 44100.0;
    if (seconds < 0.001) return -32768;

    return static_cast<int16_t>(1200.0 * std::log2(seconds));
}




struct CachedSample {
    std::shared_ptr<SFSample> sample;
    bool loopEnabled;
};

bool Sf2Exporter::exportToSf2(const QString& path, const Bank& bank, BDParser* bd) {
    SoundFont sf2;
    sf2.set_sound_engine("Emu10k1");
    sf2.set_bank_name("PS2snd Export");
    sf2.set_rom_name("ROM");

    std::map<uint32_t, CachedSample> sampleCache;

    for (const auto& prog : bank.programs) {
        if (!prog) continue;

        std::shared_ptr<SFInstrument> sfInst = sf2.NewInstrument(prog->name);
        std::vector<bool> processed(prog->tones.size(), false);

        for (size_t i = 0; i < prog->tones.size(); ++i) {
            if (processed[i]) continue;
            processed[i] = true;
            const auto& t1 = prog->tones[i];

            int pairIdx = -1;
            bool isStereoPair = false;

            // Simple Stereo pairing logic
            if (prog->is_layered && (i + 1 < prog->tones.size())) {
                const auto& t2 = prog->tones[i+1];
                bool keysMatch = (t1.min_note == t2.min_note && t1.max_note == t2.max_note);
                if (keysMatch) {
                    pairIdx = i + 1;
                    isStereoPair = true;
                    processed[pairIdx] = true;
                }
            }

            auto addZone = [&](const Tone& t, int forcedPan = -1) {
                std::shared_ptr<SFSample> sfSample;
                bool isLooping = false;

                if (sampleCache.count(t.bd_offset)) {
                    sfSample = sampleCache[t.bd_offset].sample;
                    isLooping = sampleCache[t.bd_offset].loopEnabled;
                } else {
                    std::vector<u8> raw = bd->get_adpcm_block(t.bd_offset);
                    if (raw.empty()) return;

                    DecodedSample res = BDParser::decode_adpcm(raw, t.sample_rate);
                    if (res.pcm.empty()) return;

                    // sf2cute requires non-zero loop size
                    u32 ls = res.loop_start;
                    u32 le = (res.loop_end > ls) ? res.loop_end : res.pcm.size();
                    if (le >= res.pcm.size()) le = res.pcm.size() - 1;

                    sfSample = sf2.NewSample(
                        "Smp_" + std::to_string(t.bd_offset),
                                             res.pcm, ls, le, res.sample_rate,
                                             t.root_key > 0 ? t.root_key : 60,
                                             t.pitch_fine
                    );
                    sampleCache[t.bd_offset] = { sfSample, res.looping };
                    isLooping = res.looping;
                }

                SFInstrumentZone zone(sfSample);
                zone.SetGenerator(SFGeneratorItem(SFGenerator::kSampleModes,
                                                  uint16_t(isLooping ? SampleMode::kLoopContinuously : SampleMode::kNoLoop)));

                u8 kMin = t.min_note; u8 kMax = t.max_note;
                if (kMin > kMax) std::swap(kMin, kMax);
                zone.SetGenerator(SFGeneratorItem(SFGenerator::kKeyRange, RangesType(kMin, kMax)));

                int panVal = forcedPan;
                if (panVal == -1) {
                    panVal = (int(t.pan) - 64) * 10;
                }

                zone.SetGenerator(SFGeneratorItem(SFGenerator::kPan, std::clamp(panVal, -500, 500)));

                u32 reg = ((u32)t.adsr2 << 16) | t.adsr1;

                zone.SetGenerator(SFGeneratorItem(SFGenerator::kAttackVolEnv,
                                                  HardwareADSR::simulate_timecents(reg, HardwareADSR::Phase::Attack)));
                zone.SetGenerator(SFGeneratorItem(SFGenerator::kDecayVolEnv,
                                                  HardwareADSR::simulate_timecents(reg, HardwareADSR::Phase::Decay)));
                zone.SetGenerator(SFGeneratorItem(SFGenerator::kReleaseVolEnv,
                                                  HardwareADSR::simulate_timecents(reg, HardwareADSR::Phase::Release)));
                //dunno what this is doing
                u32 susVal = get_bits(reg, 0, 4);
                uint16_t sustainAtten = static_cast<uint16_t>((15 - susVal) * 66);
                if (susVal == 0) sustainAtten = 1440;
                zone.SetGenerator(SFGeneratorItem(SFGenerator::kSustainVolEnv, sustainAtten));

                sfInst->AddZone(std::move(zone));
            };

            if (isStereoPair) {
                const auto& t2 = prog->tones[pairIdx];
                addZone(t1, -500);
                addZone(t2, 500);
            } else {
                addZone(t1);
            }
        }

        std::shared_ptr<SFPreset> preset = sf2.NewPreset("Preset " + std::to_string(prog->id), prog->id, 0);
        SFPresetZone pZone(sfInst);
        pZone.SetGenerator(SFGeneratorItem(SFGenerator::kKeyRange, RangesType(0, 127)));
        preset->AddZone(std::move(pZone));
    }

    try {
        std::ofstream ofs(path.toStdString(), std::ios::binary);
        sf2.Write(ofs);
        return true;
    } catch (...) {
        return false;
    }
}
