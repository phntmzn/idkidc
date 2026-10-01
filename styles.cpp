/*
================================================================
C++ MUSIC THEORY + LATIN RHYTHM MIDI GENERATOR
================================================================

macOS COMPILE:

    clang++ -std=c++20 -O3 main.cpp -o midi_generator

RUN:

    ./midi_generator

OPTIONAL:

    ./midi_generator tresillo
    ./midi_generator tresillo_slow
    ./midi_generator cinquillo
    ./midi_generator habanera
    ./midi_generator son_3_2
    ./midi_generator son_2_3

OUTPUT:

    MIDI_Output/
        tresillo_000001.mid
        tresillo_000002.mid
        ...

CONFIGURATION:

    FILE_COUNT = number of MIDI files
    BARS       = length of each song
    BPM        = tempo

RHYTHMIC STYLES:

    Tresillo
    Tresillo Slow
    Cinquillo
    Habanera
    Son Clave 3-2
    Son Clave 2-3

HARMONY:

    Major
    Natural Minor
    Harmonic Minor
    Melodic Minor

    Triads
    7th chords
    6th chords
    9th chords

    Inversions
    Bass roots
    Bass fifths
    Bass octaves

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
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ================================================================
// CONFIGURATION
// ================================================================

constexpr int FILE_COUNT = 1000;
constexpr int BARS = 32;
constexpr int BPM = 150;

constexpr int TICKS_PER_BEAT = 480;
constexpr int BEATS_PER_BAR = 4;
constexpr int TICKS_PER_BAR =
    TICKS_PER_BEAT * BEATS_PER_BAR;

constexpr int SIXTEENTH =
    TICKS_PER_BEAT / 4;

constexpr int BASS_CHANNEL = 0;
constexpr int HARMONY_CHANNEL = 1;

constexpr int BASS_OCTAVE = 1;
constexpr int HARMONY_OCTAVE = 3;

// ================================================================
// RHYTHM STYLE
// ================================================================

enum class RhythmStyle
{
    Tresillo,
    TresilloSlow,
    Cinquillo,
    Habanera,
    Son32,
    Son23
};

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
// SCALE
// ================================================================

struct Scale
{
    std::string name;
    std::vector<int> intervals;
};

// ================================================================
// CHORD
// ================================================================

enum class ChordType
{
    Triad,
    Seventh,
    Sixth,
    Ninth
};

struct Chord
{
    int root;
    std::vector<int> intervals;
    ChordType type;
    int degree;
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
// MIDI BIG-ENDIAN
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
// MIDI VLQ
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
// ADD META EVENT
// ================================================================

void add_meta(
    std::vector<MidiEvent>& events,
    uint32_t tick,
    uint8_t type,
    const std::vector<uint8_t>& data
)
{
    // Not used in normal note sorting.
    // Kept separate because this generator primarily
    // uses MIDI channel events.
    (void)events;
    (void)tick;
    (void)type;
    (void)data;
}

// ================================================================
// ADD NOTE
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
// SCALE CHORD BUILDERS
// ================================================================

std::vector<int> triad(
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

    return {
        0,
        third - root,
        fifth - root
    };
}

std::vector<int> seventh(
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

    int seventh_note =
        scale.intervals[
            (degree + 6) % 7
        ];

    if (degree + 2 >= 7)
        third += 12;

    if (degree + 4 >= 7)
        fifth += 12;

    if (degree + 6 >= 7)
        seventh_note += 12;

    return {
        0,
        third - root,
        fifth - root,
        seventh_note - root
    };
}

std::vector<int> sixth(
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

    int sixth_note =
        scale.intervals[
            (degree + 5) % 7
        ];

    if (degree + 2 >= 7)
        third += 12;

    if (degree + 4 >= 7)
        fifth += 12;

    if (degree + 5 >= 7)
        sixth_note += 12;

    return {
        0,
        third - root,
        fifth - root,
        sixth_note - root
    };
}

std::vector<int> ninth(
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

    int seventh_note =
        scale.intervals[
            (degree + 6) % 7
        ];

    int ninth_note =
        scale.intervals[
            (degree + 1) % 7
        ];

    if (degree + 2 >= 7)
        third += 12;

    if (degree + 4 >= 7)
        fifth += 12;

    if (degree + 6 >= 7)
        seventh_note += 12;

    ninth_note += 12;

    return {
        0,
        third - root,
        fifth - root,
        seventh_note - root,
        ninth_note - root
    };
}

// ================================================================
// ALL CHORDS
// ================================================================

std::vector<Chord> build_chords(
    const Scale& scale
)
{
    std::vector<Chord> result;

    for (int degree = 0; degree < 7; ++degree)
    {
        result.push_back({
            scale.intervals[degree],
            triad(scale, degree),
            ChordType::Triad,
            degree
        });

        result.push_back({
            scale.intervals[degree],
            seventh(scale, degree),
            ChordType::Seventh,
            degree
        });

        result.push_back({
            scale.intervals[degree],
            sixth(scale, degree),
            ChordType::Sixth,
            degree
        });

        result.push_back({
            scale.intervals[degree],
            ninth(scale, degree),
            ChordType::Ninth,
            degree
        });
    }

    return result;
}

// ================================================================
// RHYTHM NAME
// ================================================================

std::string rhythm_name(
    RhythmStyle style
)
{
    switch (style)
    {
        case RhythmStyle::Tresillo:
            return "tresillo";

        case RhythmStyle::TresilloSlow:
            return "tresillo_slow";

        case RhythmStyle::Cinquillo:
            return "cinquillo";

        case RhythmStyle::Habanera:
            return "habanera";

        case RhythmStyle::Son32:
            return "son_3_2";

        case RhythmStyle::Son23:
            return "son_2_3";
    }

    return "unknown";
}

// ================================================================
// RHYTHMIC ATTACK POSITIONS
//
// Positions are 16th-note subdivisions.
//
// Tresillo:
//     3 + 3 + 2
//
// Cinquillo:
//     2 + 1 + 2 + 1 + 2
//
// Habanera:
//     dotted-eighth + sixteenth + eighth + eighth
//
// Son clave:
//     3-2 / 2-3 across TWO bars
// ================================================================

std::vector<int> rhythm_positions(
    RhythmStyle style,
    int bar
)
{
    switch (style)
    {
        case RhythmStyle::Tresillo:

            return {
                0,
                3,
                6
            };

        case RhythmStyle::TresilloSlow:

            return {
                0,
                6,
                12
            };

        case RhythmStyle::Cinquillo:

            return {
                0,
                3,
                6,
                8,
                11
            };

        case RhythmStyle::Habanera:

            return {
                0,
                3,
                6,
                10
            };

        case RhythmStyle::Son32:

            if (bar % 2 == 0)
            {
                // 3-side
                return {
                    0,
                    3,
                    6
                };
            }
            else
            {
                // 2-side
                return {
                    10,
                    12
                };
            }

        case RhythmStyle::Son23:

            if (bar % 2 == 0)
            {
                // 2-side
                return {
                    0,
                    6
                };
            }
            else
            {
                // 3-side
                return {
                    8,
                    11,
                    14
                };
            }
    }

    return {};
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

        int first =
            notes.front();

        notes.erase(
            notes.begin()
        );

        notes.push_back(
            first + 12
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
//
// Bass follows the harmonic root but uses the rhythmic style.
// ================================================================

void add_bass_pattern(
    std::vector<MidiEvent>& events,
    const Chord& chord,
    RhythmStyle style,
    int bar,
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

    auto positions =
        rhythm_positions(
            style,
            bar
        );

    for (size_t i = 0;
         i < positions.size();
         ++i)
    {
        int pitch = root;

        // Vary bass articulation.
        switch (i % 4)
        {
            case 0:
                pitch = root;
                break;

            case 1:
                pitch = fifth;
                break;

            case 2:
                pitch = octave;
                break;

            default:
                pitch = root;
                break;
        }

        // Occasional chromatic approach.
        if (rng() % 12 == 0)
        {
            pitch +=
                (rng() % 2 == 0)
                    ? 1
                    : -1;
        }

        uint32_t start =
            bar * TICKS_PER_BAR +
            positions[i] * SIXTEENTH;

        uint32_t duration =
            SIXTEENTH * 2;

        add_note(
            events,
            BASS_CHANNEL,
            pitch,
            95 + rng() % 25,
            start,
            duration
        );
    }
}

// ================================================================
// HARMONY RHYTHM
// ================================================================

void add_harmony_pattern(
    std::vector<MidiEvent>& events,
    const Chord& chord,
    RhythmStyle style,
    int bar,
    std::mt19937& rng
)
{
    auto positions =
        rhythm_positions(
            style,
            bar
        );

    for (size_t i = 0;
         i < positions.size();
         ++i)
    {
        uint32_t start =
            bar * TICKS_PER_BAR +
            positions[i] * SIXTEENTH;

        uint32_t duration =
            SIXTEENTH * 2;

        // Longer chord stabs on stronger attacks.
        if (i == 0)
        {
            duration =
                SIXTEENTH * 3;
        }

        int inversion =
            rng() %
            static_cast<int>(
                chord.intervals.size()
            );

        int velocity =
            70 + rng() % 45;

        add_chord(
            events,
            chord,
            start,
            duration,
            inversion,
            velocity
        );
    }
}

// ================================================================
// PROGRESSION
// ================================================================

std::vector<Chord> make_progression(
    const std::vector<Chord>& chords,
    std::mt19937& rng
)
{
    /*
        Choose harmonic regions by scale degree.

        We still allow every chord family:
            triad
            7th
            6th
            9th

        Root movement is weighted toward
        musically common interval relationships.
    */

    int length =
        4 + rng() % 5;

    std::vector<Chord> progression;

    // Start on tonic.
    std::vector<const Chord*> tonic_choices;

    for (const auto& chord : chords)
    {
        if (chord.degree == 0)
            tonic_choices.push_back(&chord);
    }

    progression.push_back(
        *tonic_choices[
            rng() % tonic_choices.size()
        ]
    );

    while (
        static_cast<int>(
            progression.size()
        ) < length
    )
    {
        const Chord& previous =
            progression.back();

        std::vector<const Chord*>
            candidates;

        for (const auto& chord : chords)
        {
            int movement =
                std::abs(
                    chord.root -
                    previous.root
                ) % 12;

            if (
                movement == 0 ||
                movement == 2 ||
                movement == 3 ||
                movement == 4 ||
                movement == 5 ||
                movement == 7
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

        progression.push_back(
            *candidates[
                rng() % candidates.size()
            ]
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
    RhythmStyle style,
    std::mt19937& rng
)
{
    auto progression =
        make_progression(
            chords,
            rng
        );

    /*
        Son clave is naturally a 2-bar cycle.
        Other patterns repeat every bar.
    */

    int progression_length =
        static_cast<int>(
            progression.size()
        );

    for (int bar = 0;
         bar < BARS;
         ++bar)
    {
        int progression_position;

        if (
            style == RhythmStyle::Son32 ||
            style == RhythmStyle::Son23
        )
        {
            progression_position =
                (bar / 2) %
                progression_length;
        }
        else
        {
            progression_position =
                bar %
                progression_length;
        }

        const Chord& chord =
            progression[
                progression_position
            ];

        add_harmony_pattern(
            harmony,
            chord,
            style,
            bar,
            rng
        );

        add_bass_pattern(
            bass,
            chord,
            style,
            bar,
            rng
        );
    }
}

// ================================================================
// TRACK WITH TEMPO
// ================================================================

std::vector<uint8_t> build_track(
    std::vector<MidiEvent> events,
    bool tempo_track = false
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

            return a.status < b.status;
        }
    );

    std::vector<uint8_t> track;

    uint32_t previous_tick = 0;

    for (const auto& event : events)
    {
        uint32_t delta =
            event.tick -
            previous_tick;

        auto encoded =
            vlq(delta);

        track.insert(
            track.end(),
            encoded.begin(),
            encoded.end()
        );

        track.push_back(
            event.status
        );

        track.push_back(
            event.data1
        );

        track.push_back(
            event.data2
        );

        previous_tick =
            event.tick;
    }

    // Tempo meta event.
    if (tempo_track)
    {
        // 60,000,000 / BPM
        uint32_t microseconds =
            60000000 / BPM;

        auto tempo_bytes =
            vlq(0);

        track.insert(
            track.end(),
            tempo_bytes.begin(),
            tempo_bytes.end()
        );

        track.push_back(0xff);
        track.push_back(0x51);
        track.push_back(0x03);

        track.push_back(
            static_cast<uint8_t>(
                (microseconds >> 16) & 0xff
            )
        );

        track.push_back(
            static_cast<uint8_t>(
                (microseconds >> 8) & 0xff
            )
        );

        track.push_back(
            static_cast<uint8_t>(
                microseconds & 0xff
            )
        );
    }

    // End of track.
    track.push_back(0x00);
    track.push_back(0xff);
    track.push_back(0x2f);
    track.push_back(0x00);

    return track;
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
            "Cannot create: " +
            filename.string()
        );
    }

    auto harmony_track =
        build_track(
            harmony
        );

    auto bass_track =
        build_track(
            bass
        );

    // ------------------------------------------------------------
    // MIDI HEADER
    // ------------------------------------------------------------

    file.write("MThd", 4);

    write_u32(
        file,
        6
    );

    // Format 1
    write_u16(
        file,
        1
    );

    // Two tracks
    write_u16(
        file,
        2
    );

    write_u16(
        file,
        TICKS_PER_BEAT
    );

    // ------------------------------------------------------------
    // HARMONY TRACK
    // ------------------------------------------------------------

    file.write(
        "MTrk",
        4
    );

    write_u32(
        file,
        static_cast<uint32_t>(
            harmony_track.size()
        )
    );

    file.write(
        reinterpret_cast<
            const char*
        >(
            harmony_track.data()
        ),
        static_cast<
            std::streamsize
        >(
            harmony_track.size()
        )
    );

    // ------------------------------------------------------------
    // BASS TRACK
    // ------------------------------------------------------------

    file.write(
        "MTrk",
        4
    );

    write_u32(
        file,
        static_cast<uint32_t>(
            bass_track.size()
        )
    );

    file.write(
        reinterpret_cast<
            const char*
        >(
            bass_track.data()
        ),
        static_cast<
            std::streamsize
        >(
            bass_track.size()
        )
    );
}

// ================================================================
// PARSE STYLE
// ================================================================

RhythmStyle parse_style(
    const std::string& name
)
{
    if (name == "tresillo")
        return RhythmStyle::Tresillo;

    if (name == "tresillo_slow")
        return RhythmStyle::TresilloSlow;

    if (name == "cinquillo")
        return RhythmStyle::Cinquillo;

    if (name == "habanera")
        return RhythmStyle::Habanera;

    if (name == "son_3_2")
        return RhythmStyle::Son32;

    if (name == "son_2_3")
        return RhythmStyle::Son23;

    throw std::runtime_error(
        "Unknown rhythm style: " +
        name
    );
}

// ================================================================
// MAIN
// ================================================================

int main(
    int argc,
    char* argv[]
)
{
    fs::path output =
        "MIDI_Output";

    fs::create_directories(
        output
    );

    std::random_device rd;

    std::mt19937 rng(rd());

    std::vector<RhythmStyle> styles;

    // ------------------------------------------------------------
    // STYLE SELECTION
    // ------------------------------------------------------------

    if (argc > 1)
    {
        styles.push_back(
            parse_style(argv[1])
        );
    }
    else
    {
        styles =
        {
            RhythmStyle::Tresillo,
            RhythmStyle::TresilloSlow,
            RhythmStyle::Cinquillo,
            RhythmStyle::Habanera,
            RhythmStyle::Son32,
            RhythmStyle::Son23
        };
    }

    std::cout
        << "========================================\n"
        << "C++ MUSIC THEORY MIDI GENERATOR\n"
        << "========================================\n\n";

    std::cout
        << "Files per style: "
        << FILE_COUNT
        << '\n';

    std::cout
        << "Bars: "
        << BARS
        << '\n';

    std::cout
        << "BPM: "
        << BPM
        << "\n\n";

    int file_number = 0;

    // ------------------------------------------------------------
    // GENERATE EACH RHYTHMIC STYLE
    // ------------------------------------------------------------

    for (RhythmStyle style : styles)
    {
        std::string style_name =
            rhythm_name(style);

        std::cout
            << "STYLE: "
            << style_name
            << '\n';

        // Randomly choose a scale for this batch.
        for (int i = 0;
             i < FILE_COUNT;
             ++i)
        {
            const Scale& scale =
                SCALES[
                    rng() % SCALES.size()
                ];

            auto chords =
                build_chords(
                    scale
                );

            std::vector<MidiEvent>
                harmony;

            std::vector<MidiEvent>
                bass;

            generate_song(
                harmony,
                bass,
                chords,
                style,
                rng
            );

            std::string filename =
                style_name +
                "_" +
                scale.name +
                "_" +
                std::to_string(i + 1) +
                ".mid";

            fs::path path =
                output /
                filename;

            write_midi(
                path,
                harmony,
                bass
            );

            ++file_number;

            if (
                (i + 1) % 100 == 0
            )
            {
                std::cout
                    << "  "
                    << i + 1
                    << " / "
                    << FILE_COUNT
                    << '\n';
            }
        }

        std::cout << '\n';
    }

    std::cout
        << "========================================\n"
        << "DONE\n"
        << "========================================\n";

    std::cout
        << "Total files: "
        << file_number
        << '\n';

    std::cout
        << "Output: "
        << fs::absolute(output)
        << '\n';

    return 0;
}
