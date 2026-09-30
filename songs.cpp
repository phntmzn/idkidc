/*
============================================================
C++ THEORY-BASED MIDI GENERATOR
============================================================

macOS COMPILE:

    clang++ -std=c++20 -O3 main.cpp -o midi_generator

RUN:

    ./midi_generator

The program creates:

    MIDI_Output/
        fsharp_minor_1.mid
        fsharp_minor_2.mid
        ...
        fsharp_minor_1000.mid

CHANGE NUMBER OF FILES:

    constexpr int FILE_COUNT = 10000;

CHANGE LENGTH:

    constexpr int BARS = 64;

MUSIC THEORY:

    Key:
        F# harmonic minor

    Scale:
        F# G# A B C# D E#

    Includes:
        - Chord progressions
        - Power chords
        - Bass root movement
        - Bass fifths
        - Bass octaves
        - 16th-note rhythms
        - Guitar track
        - Bass track
        - Multiple rhythmic patterns
        - Multiple chord progressions
        - Stochastic variation

NO EXTERNAL MIDI LIBRARY REQUIRED.

C++20 STANDARD LIBRARY ONLY.

============================================================
*/

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>
#include <algorithm>

namespace fs = std::filesystem;

constexpr int FILE_COUNT = 1000;
constexpr int BARS = 64;

constexpr int TICKS_PER_BEAT = 480;
constexpr int BEATS_PER_BAR = 4;
constexpr int TICKS_PER_BAR =
    TICKS_PER_BEAT * BEATS_PER_BAR;

constexpr int TEMPO_BPM = 150;

constexpr int BASS_OCTAVE = 1;
constexpr int GUITAR_OCTAVE = 3;

constexpr int BASS_CHANNEL = 0;
constexpr int GUITAR_CHANNEL = 1;

// F#
constexpr int KEY = 6;

struct MidiEvent {
    uint32_t tick;
    uint8_t status;
    uint8_t data1;
    uint8_t data2;
};

void write_u16(
    std::ofstream& file,
    uint16_t value
)
{
    file.put(
        static_cast<char>((value >> 8) & 0xff)
    );

    file.put(
        static_cast<char>(value & 0xff)
    );
}

void write_u32(
    std::ofstream& file,
    uint32_t value
)
{
    file.put(
        static_cast<char>((value >> 24) & 0xff)
    );

    file.put(
        static_cast<char>((value >> 16) & 0xff)
    );

    file.put(
        static_cast<char>((value >> 8) & 0xff)
    );

    file.put(
        static_cast<char>(value & 0xff)
    );
}

std::vector<uint8_t> vlq(uint32_t value)
{
    std::vector<uint8_t> bytes;

    bytes.push_back(
        static_cast<uint8_t>(value & 0x7f)
    );

    while ((value >>= 7) != 0)
    {
        bytes.push_back(
            static_cast<uint8_t>(
                (value & 0x7f) | 0x80
            )
        );
    }

    std::reverse(
        bytes.begin(),
        bytes.end()
    );

    return bytes;
}

std::vector<uint8_t> build_track(
    std::vector<MidiEvent> events
)
{
    std::sort(
        events.begin(),
        events.end(),
        [](const MidiEvent& a, const MidiEvent& b)
        {
            return a.tick < b.tick;
        }
    );

    std::vector<uint8_t> track;

    uint32_t previous_tick = 0;

    for (const auto& event : events)
    {
        uint32_t delta =
            event.tick - previous_tick;

        auto encoded = vlq(delta);

        track.insert(
            track.end(),
            encoded.begin(),
            encoded.end()
        );

        track.push_back(event.status);
        track.push_back(event.data1);
        track.push_back(event.data2);

        previous_tick = event.tick;
    }

    track.push_back(0x00);
    track.push_back(0xff);
    track.push_back(0x2f);
    track.push_back(0x00);

    return track;
}

void add_note(
    std::vector<MidiEvent>& events,
    int channel,
    int pitch,
    int velocity,
    uint32_t start,
    uint32_t duration
)
{
    pitch = std::clamp(pitch, 0, 127);
    velocity = std::clamp(velocity, 1, 127);

    uint8_t note_on =
        static_cast<uint8_t>(
            0x90 | channel
        );

    uint8_t note_off =
        static_cast<uint8_t>(
            0x80 | channel
        );

    events.push_back({
        start,
        note_on,
        static_cast<uint8_t>(pitch),
        static_cast<uint8_t>(velocity)
    });

    events.push_back({
        start + duration,
        note_off,
        static_cast<uint8_t>(pitch),
        0
    });
}

// F# harmonic minor:
//
// F# G# A B C# D E#
//
// 0  2  3 5 7 8 11

constexpr std::array<int, 7> SCALE = {
    0, 2, 3, 5, 7, 8, 11
};

struct Chord
{
    int root;
    std::array<int, 4> intervals;
};

const std::array<Chord, 7> CHORDS = {

    Chord{0,  {0, 3, 7, 0}},   // F#m
    Chord{2,  {0, 3, 7, 0}},   // G#dim
    Chord{3,  {0, 4, 7, 0}},   // A
    Chord{5,  {0, 3, 7, 0}},   // Bm
    Chord{7,  {0, 4, 7, 0}},   // C#
    Chord{8,  {0, 4, 7, 0}},   // D
    Chord{11, {0, 3, 6, 0}}    // E#dim
};

const std::array<std::array<int, 8>, 4> PROGRESSIONS = {

    std::array<int, 8>{
        0, 5, 2, 4,
        0, 5, 4, 0
    },

    std::array<int, 8>{
        0, 3, 5, 4,
        0, 5, 2, 4
    },

    std::array<int, 8>{
        0, 5, 3, 6,
        0, 3, 4, 0
    },

    std::array<int, 8>{
        0, 4, 5, 3,
        0, 5, 4, 0
    }
};

void add_power_chord(
    std::vector<MidiEvent>& events,
    int root,
    uint32_t start,
    uint32_t duration,
    int velocity
)
{
    int root_pitch =
        12 * GUITAR_OCTAVE +
        KEY +
        root;

    add_note(
        events,
        GUITAR_CHANNEL,
        root_pitch,
        velocity,
        start,
        duration
    );

    add_note(
        events,
        GUITAR_CHANNEL,
        root_pitch + 7,
        velocity,
        start,
        duration
    );

    add_note(
        events,
        GUITAR_CHANNEL,
        root_pitch + 12,
        velocity,
        start,
        duration
    );
}

void add_bass_pattern(
    std::vector<MidiEvent>& events,
    int root,
    uint32_t bar_start,
    std::mt19937& rng
)
{
    constexpr int STEP =
        TICKS_PER_BEAT / 2;

    int base =
        12 * BASS_OCTAVE +
        KEY +
        root;

    std::array<int, 8> pattern = {

        0,
        12,
        7,
        12,

        0,
        12,
        7,
        0
    };

    if (rng() % 4 == 0)
    {
        std::reverse(
            pattern.begin(),
            pattern.end()
        );
    }

    for (int i = 0; i < 8; ++i)
    {
        int velocity =
            95 + static_cast<int>(rng() % 25);

        add_note(
            events,
            BASS_CHANNEL,
            base + pattern[i],
            velocity,
            bar_start + i * STEP,
            STEP - 20
        );
    }
}

void add_guitar_pattern(
    std::vector<MidiEvent>& events,
    int root,
    uint32_t bar_start,
    std::mt19937& rng
)
{
    constexpr int SIXTEENTH =
        TICKS_PER_BEAT / 4;

    const std::array<std::array<int, 16>, 4>
        patterns = {

        std::array<int, 16>{
            1,1,1,1,
            1,1,1,1,
            1,1,1,1,
            1,1,1,1
        },

        std::array<int, 16>{
            1,0,1,0,
            1,0,1,0,
            1,1,0,1,
            1,0,1,0
        },

        std::array<int, 16>{
            1,1,0,1,
            1,0,1,1,
            1,1,0,1,
            1,0,0,1
        },

        std::array<int, 16>{
            1,0,0,1,
            1,0,1,0,
            1,1,0,0,
            1,0,1,1
        }
    };

    const auto& pattern =
        patterns[rng() % patterns.size()];

    for (int i = 0; i < 16; ++i)
    {
        if (!pattern[i])
            continue;

        int velocity =
            75 + static_cast<int>(rng() % 45);

        add_power_chord(
            events,
            root,
            bar_start + i * SIXTEENTH,
            SIXTEENTH - 15,
            velocity
        );
    }
}

void generate_song(
    std::vector<MidiEvent>& guitar,
    std::vector<MidiEvent>& bass,
    std::mt19937& rng
)
{
    const auto& progression =
        PROGRESSIONS[
            rng() % PROGRESSIONS.size()
        ];

    for (int bar = 0; bar < BARS; ++bar)
    {
        int chord_index =
            progression[
                (bar / 2) % progression.size()
            ];

        const Chord& chord =
            CHORDS[chord_index];

        uint32_t bar_start =
            bar * TICKS_PER_BAR;

        add_guitar_pattern(
            guitar,
            chord.root,
            bar_start,
            rng
        );

        add_bass_pattern(
            bass,
            chord.root,
            bar_start,
            rng
        );
    }
}

void write_midi(
    const fs::path& filename,
    const std::vector<MidiEvent>& guitar,
    const std::vector<MidiEvent>& bass
)
{
    std::ofstream file(
        filename,
        std::ios::binary
    );

    if (!file)
    {
        throw std::runtime_error(
            "Cannot create " +
            filename.string()
        );
    }

    auto guitar_track =
        build_track(guitar);

    auto bass_track =
        build_track(bass);

    file.write("MThd", 4);

    write_u32(file, 6);
    write_u16(file, 1);
    write_u16(file, 2);

    write_u16(
        file,
        TICKS_PER_BEAT
    );

    file.write("MTrk", 4);

    write_u32(
        file,
        static_cast<uint32_t>(
            guitar_track.size()
        )
    );

    file.write(
        reinterpret_cast<const char*>(
            guitar_track.data()
        ),
        static_cast<std::streamsize>(
            guitar_track.size()
        )
    );

    file.write("MTrk", 4);

    write_u32(
        file,
        static_cast<uint32_t>(
            bass_track.size()
        )
    );

    file.write(
        reinterpret_cast<const char*>(
            bass_track.data()
        ),
        static_cast<std::streamsize>(
            bass_track.size()
        )
    );
}

int main()
{
    const fs::path output =
        "MIDI_Output";

    fs::create_directories(output);

    std::random_device rd;
    std::mt19937 rng(rd());

    std::cout
        << "C++ Theory MIDI Generator\n"
        << "Key: F# harmonic minor\n"
        << "Tempo: "
        << TEMPO_BPM
        << " BPM\n"
        << "Bars: "
        << BARS
        << '\n'
        << "Files: "
        << FILE_COUNT
        << "\n\n";

    for (int i = 0; i < FILE_COUNT; ++i)
    {
        std::vector<MidiEvent> guitar;
        std::vector<MidiEvent> bass;

        generate_song(
            guitar,
            bass,
            rng
        );

        fs::path filename =
            output /
            (
                "fsharp_minor_" +
                std::to_string(i + 1) +
                ".mid"
            );

        write_midi(
            filename,
            guitar,
            bass
        );

        if ((i + 1) % 100 == 0)
        {
            std::cout
                << "Generated "
                << i + 1
                << " / "
                << FILE_COUNT
                << '\n';
        }
    }

    std::cout
        << "\nDone.\n"
        << "Output: "
        << fs::absolute(output)
        << '\n';

    return 0;
}
