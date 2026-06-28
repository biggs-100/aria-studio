#ifndef ARIA_TIMELINE_H
#define ARIA_TIMELINE_H

#include <cstdint>

namespace aria {

/// Timeline — sample-accurate timeline with tempo map and time signature.
class Timeline {
public:
    Timeline();
    ~Timeline();

    void set_tempo(double bpm);
    double tempo() const;

    void set_time_signature(int numerator, int denominator);
    void time_signature(int& numerator, int& denominator) const;

    void set_position(uint64_t samples);
    uint64_t position() const;

private:
    double bpm_ = 120.0;
    int ts_num_ = 4;
    int ts_den_ = 4;
    uint64_t position_ = 0;
};

} // namespace aria

#endif // ARIA_TIMELINE_H
