#pragma once

namespace rta
{

/** Tilt mode for frequency compensation */
enum class TiltMode
{
    Flat = 0,   // 0 dB/oct
    Pink = 1,   // +3 dB/oct (compensate pink noise downward slope)
    White = 2   // -3 dB/oct (compensate white noise upward slope)
};

} // namespace rta
