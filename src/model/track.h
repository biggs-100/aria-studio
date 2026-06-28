#ifndef ARIA_TRACK_H
#define ARIA_TRACK_H

#include <cstdint>
#include <string>

namespace aria {

enum class TrackType {
    Audio,
    MIDI,
    Group,
    VCA,
    Return,
    Master
};

/// Track — base track model for all track types.
class Track {
public:
    explicit Track(TrackType type);
    ~Track() = default;

    uint64_t id() const;
    void set_name(const std::string& name);
    std::string name() const;
    TrackType type() const;

    void set_volume(double db);
    double volume() const;
    void set_pan(double pan);
    double pan() const;

    void set_muted(bool muted);
    bool is_muted() const;
    void set_soloed(bool soloed);
    bool is_soloed() const;

private:
    uint64_t id_;
    std::string name_;
    TrackType type_;
    double volume_db_ = 0.0;
    double pan_ = 0.0;
    bool muted_ = false;
    bool soloed_ = false;
};

} // namespace aria

#endif // ARIA_TRACK_H
