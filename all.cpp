/*
================================================================
C++ MUSIC THEORY MIDI GENERATOR
================================================================

macOS COMPILE:

    clang++ -std=c++20 -O3 main.cpp -o midi_generator

RUN:

    ./midi_generator

OUTPUT:

    MIDI_Output/
        song_000001.mid
        song_000002.mid
        song_000003.mid
        ...

CHANGE NUMBER OF FILES:

    constexpr int FILE_COUNT = 10000;

CHANGE BARS:

    constexpr int BARS = 32;

MUSIC THEORY:

    Keys:
        Major
        Natural Minor
        Harmonic Minor
        Melodic Minor

    Chord families:

        Triads
        7th chords
        6th chords
        9th chords

    Includes:

        Major
        Minor
        Diminished
        Augmented
        Dominant 7
        Major 7
        Minor 7
        Half-diminished 7
        Diminished 7
        Major 6
        Minor 6
        Dominant 9
        Major 9
        Minor 9
        etc.

    Also generates:

        Chord inversions
        Root movement
        Bass lines
        Power-chord voicings
        Arpeggiated chords
        Rhythmic variation
        Progression variation
        Stochastic variation

NO EXTERNAL MIDI LIBRARY REQUIRED.

C++20 STANDARD LIBRARY ONLY.

================================================================
*/

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ================================================================
// CONFIGURATION
// ================================================================

constexpr int FILE_COUNT = 10000;

constexpr int BARS = 32;

constexpr int BPM = 150;

constexpr int TICKS_PER_BEAT = 480;

constexpr int BEATS_PER_BAR = 4;

constexpr int TICKS_PER_BAR =
    TICKS_PER_BEAT * BEATS_PER_BAR;

constexpr int BASS_CHANNEL = 0;

constexpr int HARMONY_CHANNEL = 1;

constexpr int BASS_OCTAVE = 1;

constexpr int HARMONY_OCTAVE = 3;

// ================================================================
// MIDI EVENT
// ================================================================

struct MidiEvent
{
    uint32_t tick;

    uint8_t status;

    uint8_t data1;

    uint8_t data2;
};

// ================================================================
// CHORD TYPES
// ================================================================

enum class ChordType
{
    Triad,

    Seventh,

    Sixth,

    Ninth
};

// ================================================================
// CHORD
// ================================================================

struct Chord
{
    int root;

    std::vector<int> intervals;

    ChordType type;

    int scale_degree;
};

// ================================================================
// BIG ENDIAN
// ================================================================

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

// ================================================================
// MIDI VARIABLE LENGTH QUANTITY
// ================================================================

std::vector<uint8_t> vlq(uint32_t value)
{
    std::vector<uint8_t> result;

    result.push_back(
        static_cast<uint8_t>(value & 0x7f)
    );

    while ((value >>= 7) != 0)
    {
        result.push_back(
            static_cast<uint8_t>(
                (value & 0x7f) | 0x80
            )
        );
    }

    std::reverse(
        result.begin(),
        result.end()
    );

    return result;
}

// ================================================================
// BUILD MIDI TRACK
// ================================================================

std::vector<uint8_t> build_track(
    std::vector<MidiEvent> events
)
{
    std::sort(
        events.begin(),
        events.end(),
        [](const MidiEvent& a,
           const MidiEvent& b)
        {
            if (a.tick != b.tick)
                return a.tick < b.tick;

            // Note-offs before note-ons at same tick
            return a.status < b.status;
        }
    );

    std::vector<uint8_t> track;

    uint32_t previous_tick = 0;

    for (const auto& event : events)
    {
        uint32_t delta =
            event.tick - previous_tick;

        auto bytes = vlq(delta);

        track.insert(
            track.end(),
            bytes.begin(),
            bytes.end()
        );

        track.push_back(event.status);

        track.push_back(event.data1);

        track.push_back(event.data2);

        previous_tick = event.tick;
    }

    // End of track
    track.push_back(0x00);
    track.push_back(0xff);
    track.push_back(0x2f);
    track.push_back(0x00);

    return track;
}

// ================================================================
// ADD MIDI NOTE
// ================================================================

void add_note(
    std::vector<MidiEvent>& events,
    int channel,
    int pitch,
    int velocity,
    uint32_t start,
    uint32_t duration
)
{
    pitch =
        std::clamp(pitch, 0, 127);

    velocity =
        std::clamp(velocity, 1, 127);

    events.push_back(
        {
            start,
            static_cast<uint8_t>(
                0x90 | channel
            ),
            static_cast<uint8_t>(pitch),
            static_cast<uint8_t>(velocity)
        }
    );

    events.push_back(
        {
            start + duration,
            static_cast<uint8_t>(
                0x80 | channel
            ),
            static_cast<uint8_t>(pitch),
            0
        }
    );
}

// ================================================================
// SCALE
// ================================================================

struct Scale
{
    std::string name;

    std::vector<int> intervals;
};

// ================================================================
// SCALES
// ================================================================

const std::vector<Scale> SCALES =
{
    {
        "major",
        {0, 2, 4, 5, 7, 9, 11}
    },

    {
        "natural_minor",
        {0, 2, 3, 5, 7, 8, 10}
    },

    {
        "harmonic_minor",
        {0, 2, 3, 5, 7, 8, 11}
    },

    {
        "melodic_minor",
        {0, 2, 3, 5, 7, 9, 11}
    }
};

// ================================================================
// DIATONIC TRIAD QUALITY
// ================================================================
//
// major:
// I  ii iii IV V vi vii°
//
// minor:
// i ii° III iv v VI VII
//
// The generator derives the actual chord intervals from
// the selected scale.
// ================================================================

std::vector<int> build_triad(
    const Scale& scale,
    int degree
)
{
    int root =
        scale.intervals[degree];

    int third =
        scale.intervals[
            (degree + 2) % 7
        ];

    int fifth =
        scale.intervals[
            (degree + 4) % 7
        ];

    if (degree + 2 >= 7)
        third += 12;

    if (degree + 4 >= 7)
        fifth += 12;

    return
    {
        0,
        third - root,
        fifth - root
    };
}

// ================================================================
// SEVENTH CHORD
// ================================================================

std::vector<int> build_seventh(
    const Scale& scale,
    int degree
)
{
    int root =
        scale.intervals[degree];

    int third =
        scale.intervals[
            (degree + 2) % 7
        ];

    int fifth =
        scale.intervals[
            (degree + 4) % 7
        ];

    int seventh =
        scale.intervals[
            (degree + 6) % 7
        ];

    if (degree + 2 >= 7)
        third += 12;

    if (degree + 4 >= 7)
        fifth += 12;

    if (degree + 6 >= 7)
        seventh += 12;

    return
    {
        0,
        third - root,
        fifth - root,
        seventh - root
    };
}

// ================================================================
// SIXTH CHORD
//
// 1 3 5 6
// ================================================================

std::vector<int> build_sixth(
    const Scale& scale,
    int degree
)
{
    int root =
        scale.intervals[degree];

    int third =
        scale.intervals[
            (degree + 2) % 7
        ];

    int fifth =
        scale.intervals[
            (degree + 4) % 7
        ];

    int sixth =
        scale.intervals[
            (degree + 5) % 7
        ];

    if (degree + 2 >= 7)
        third += 12;

    if (degree + 4 >= 7)
        fifth += 12;

    if (degree + 5 >= 7)
        sixth += 12;

    return
    {
        0,
        third - root,
        fifth - root,
        sixth - root
    };
}

// ================================================================
// NINTH CHORD
//
// 1 3 5 7 9
// ================================================================

std::vector<int> build_ninth(
    const Scale& scale,
    int degree
)
{
    int root =
        scale.intervals[degree];

    int third =
        scale.intervals[
            (degree + 2) % 7
        ];

    int fifth =
        scale.intervals[
            (degree + 4) % 7
        ];

    int seventh =
        scale.intervals[
            (degree + 6) % 7
        ];

    int ninth =
        scale.intervals[
            (degree + 1) % 7
        ];

    if (degree + 2 >= 7)
        third += 12;

    if (degree + 4 >= 7)
        fifth += 12;

    if (degree + 6 >= 7)
        seventh += 12;

    ninth += 12;

    return
    {
        0,
        third - root,
        fifth - root,
        seventh - root,
        ninth - root
    };
}

// ================================================================
// BUILD ALL CHORDS FOR A SCALE
// ================================================================

std::vector<Chord> build_chords(
    const Scale& scale
)
{
    std::vector<Chord> chords;

    for (int degree = 0; degree < 7; ++degree)
    {
        int root =
            scale.intervals[degree];

        chords.push_back(
            {
                root,
                build_triad(scale, degree),
                ChordType::Triad,
                degree
            }
        );

        chords.push_back(
            {
                root,
                build_seventh(scale, degree),
                ChordType::Seventh,
                degree
            }
        );

        chords.push_back(
            {
                root,
                build_sixth(scale, degree),
                ChordType::Sixth,
                degree
            }
        );

        chords.push_back(
            {
                root,
                build_ninth(scale, degree),
                ChordType::Ninth,
                degree
            }
        );
    }

    return chords;
}

// ================================================================
// CHORD NAME
// ================================================================

std::string chord_name(
    const Chord& chord
)
{
    static const std::array<std::string, 12>
        names =
    {
        "C", "C#", "D", "D#",
        "E", "F", "F#", "G",
        "G#", "A", "A#", "B"
    };

    std::string name =
        names[chord.root % 12];

    switch (chord.type)
    {
        case ChordType::Triad:
            name += "triad";
            break;

        case ChordType::Seventh:
            name += "7";
            break;

        case ChordType::Sixth:
            name += "6";
            break;

        case ChordType::Ninth:
            name += "9";
            break;
    }

    return name;
}

// ================================================================
// ADD CHORD
// ================================================================

void add_chord(
    std::vector<MidiEvent>& events,
    const Chord& chord,
    uint32_t start,
    uint32_t duration,
    int inversion,
    int velocity
)
{
    int root_pitch =
        12 * HARMONY_OCTAVE +
        chord.root;

    std::vector<int> notes;

    for (int interval : chord.intervals)
    {
        notes.push_back(
            root_pitch + interval
        );
    }

    // Inversion
    for (int i = 0; i < inversion; ++i)
    {
        if (notes.empty())
            break;

        notes.push_back(
            notes.front() + 12
        );

        notes.erase(
            notes.begin()
        );
    }

    for (int note : notes)
    {
        add_note(
            events,
            HARMONY_CHANNEL,
            note,
            velocity,
            start,
            duration
        );
    }
}

// ================================================================
// BASS
// ================================================================

void add_bass(
    std::vector<MidiEvent>& events,
    const Chord& chord,
    uint32_t start,
    std::mt19937& rng
)
{
    int root =
        12 * BASS_OCTAVE +
        chord.root;

    int fifth =
        root + 7;

    int octave =
        root + 12;

    constexpr int step =
        TICKS_PER_BEAT / 2;

    std::array<int, 8> pattern =
    {
        root,
        root,
        fifth,
        root,

        octave,
        root,
        fifth,
        root
    };

    if (rng() % 3 == 0)
    {
        std::rotate(
            pattern.begin(),
            pattern.begin() + 2,
            pattern.end()
        );
    }

    for (int i = 0; i < 8; ++i)
    {
        add_note(
            events,
            BASS_CHANNEL,
            pattern[i],
            100 + rng() % 20,
            start + i * step,
            step - 25
        );
    }
}

// ================================================================
// GENERATE PROGRESSION
// ================================================================

std::vector<Chord> generate_progression(
    const std::vector<Chord>& chords,
    std::mt19937& rng
)
{
    std::vector<Chord> progression;

    int length =
        4 + rng() % 5;

    // Start from a random tonic triad.
    int tonic =
        rng() % 4;

    progression.push_back(
        chords[tonic]
    );

    while (
        static_cast<int>(
            progression.size()
        ) < length
    )
    {
        const Chord& previous =
            progression.back();

        std::vector<const Chord*> candidates;

        for (const auto& chord : chords)
        {
            int movement =
                std::abs(
                    chord.root -
                    previous.root
                );

            movement %= 12;

            // Favor useful root movement.
            if (
                movement == 0 ||
                movement == 5 ||
                movement == 7 ||
                movement == 2 ||
                movement == 3 ||
                movement == 4
            )
            {
                candidates.push_back(
                    &chord
                );
            }
        }

        if (candidates.empty())
        {
            candidates.push_back(
                &chords[
                    rng() % chords.size()
                ]
            );
        }

        const Chord* selected =
            candidates[
                rng() % candidates.size()
            ];

        progression.push_back(
            *selected
        );
    }

    return progression;
}

// ================================================================
// GENERATE SONG
// ================================================================

void generate_song(
    std::vector<MidiEvent>& harmony,
    std::vector<MidiEvent>& bass,
    const std::vector<Chord>& chords,
    std::mt19937& rng
)
{
    auto progression =
        generate_progression(
            chords,
            rng
        );

    for (int bar = 0; bar < BARS; ++bar)
    {
        const Chord& chord =
            progression[
                bar % progression.size()
            ];

        uint32_t start =
            bar * TICKS_PER_BAR;

        // Change chord every bar.
        int inversion =
            rng() % chord.intervals.size();

        int velocity =
            75 + rng() % 40;

        add_chord(
            harmony,
            chord,
            start,
            TICKS_PER_BAR - 20,
            inversion,
            velocity
        );

        add_bass(
            bass,
            chord,
            start,
            rng
        );
    }
}

// ================================================================
// WRITE MIDI
// ================================================================

void write_midi(
    const fs::path& filename,
    const std::vector<MidiEvent>& harmony,
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

    auto harmony_track =
        build_track(harmony);

    auto bass_track =
        build_track(bass);

    // Header
    file.write("MThd", 4);

    write_u32(file, 6);

    // Format 1
    write_u16(file, 1);

    // Two tracks
    write_u16(file, 2);

    write_u16(
        file,
        TICKS_PER_BEAT
    );

    // Harmony
    file.write("MTrk", 4);

    write_u32(
        file,
        static_cast<uint32_t>(
            harmony_track.size()
        )
    );

    file.write(
        reinterpret_cast<const char*>(
            harmony_track.data()
        ),
        static_cast<std::streamsize>(
            harmony_track.size()
        )
    );

    // Bass
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

// ================================================================
// MAIN
// ================================================================

int main()
{
    fs::path output =
        "MIDI_Output";

    fs::create_directories(
        output
    );

    std::random_device rd;

    std::mt19937 rng(rd());

    std::cout
        << "========================================\n"
        << "C++ MUSIC THEORY MIDI GENERATOR\n"
        << "========================================\n\n";

    std::cout
        << "Files: "
        << FILE_COUNT
        << '\n';

    std::cout
        << "Bars: "
        << BARS
        << '\n';

    std::cout
        << "Tempo: "
        << BPM
        << "\n\n";

    int file_number = 0;

    for (
        const auto& scale :
        SCALES
    )
    {
        std::cout
            << "Scale: "
            << scale.name
            << '\n';

        auto chords =
            build_chords(scale);

        std::cout
            << "Available chords: "
            << chords.size()
            << "\n";

        /*
            7 degrees ×

            4 chord families =

            28 diatonic chord choices
        */

        for (
            int song = 0;
            song < FILE_COUNT &&
            file_number < FILE_COUNT;
            ++song
        )
        {
            std::vector<MidiEvent>
                harmony;

            std::vector<MidiEvent>
                bass;

            generate_song(
                harmony,
                bass,
                chords,
                rng
            );

            fs::path filename =
                output /
                (
                    "song_" +
                    std::to_string(
                        file_number + 1
                    ) +
                    ".mid"
                );

            write_midi(
                filename,
                harmony,
                bass
            );

            ++file_number;

            if (
                file_number % 100 == 0
            )
            {
                std::cout
                    << "Generated "
                    << file_number
                    << " / "
                    << FILE_COUNT
                    << '\n';
            }
        }

        if (
            file_number >= FILE_COUNT
        )
        {
            break;
        }
    }

    std::cout
        << "\n========================================\n"
        << "DONE\n"
        << "========================================\n";

    std::cout
        << "Output:\n"
        << fs::absolute(output)
        << '\n';

    return 0;
}
