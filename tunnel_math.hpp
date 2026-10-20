#pragma once

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <stdexcept>
#include <string>

namespace tunnel {

constexpr double Pi = 3.14159265358979323846;
constexpr std::array<const char*, 6> PaletteNames{
    "grayscale", "rainbow", "blue", "green", "yellow", "red"
};

struct Vec3 {
    double x, y, z;
    Vec3 operator+(Vec3 b) const { return {x+b.x, y+b.y, z+b.z}; }
    Vec3 operator-(Vec3 b) const { return {x-b.x, y-b.y, z-b.z}; }
    Vec3 operator*(double k) const { return {x*k, y*k, z*k}; }
};
inline double dot(Vec3 a, Vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
inline Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x};
}
inline Vec3 normalized(Vec3 a) { return a * (1.0/std::sqrt(dot(a,a))); }
inline double smoothStep(double x) {
    x = std::clamp(x, 0.0, 1.0);
    return x*x*(3.0-2.0*x);
}

struct Camera {
    Vec3 position, target, forward, right, up;
};

// Sijainti ja katseen kohde ovat kaksi eri asiaa. Kuten Pythonin pieni
// dataclass, Camera vain kokoaa nämä vektorit samaan palautusarvoon.
inline Camera cameraAt(double seconds, double distance, double sway) {
    const double t = seconds;
    const double strength = std::clamp(sway, 0.0, 1.0);
    // Säde käy lähes keskustassa ja lähellä seinää (tunnelin säde on 1).
    // Eripituiset siniaallot välttävät lyhyttä, helposti havaittavaa toistoa.
    const double radius = strength * (0.06 + 0.74 *
        (0.5 + 0.5*std::sin(t*0.31 + 0.65*std::sin(t*0.117))));
    const double orbit = -Pi/4 + t*0.39 + 0.72*std::sin(t*0.61)
        + 0.24*std::sin(t*1.07);
    const Vec3 position{radius*std::cos(orbit), radius*std::sin(orbit), distance};

    // Kohde kiertää näkymätöntä ympyrää edempänä tunnelissa. Kulman
    // lisäaallot saavat katseen hidastamaan, kiihtymään ja hetkittäin palaamaan.
    const double targetAngle = t*0.29 + 1.12*std::sin(t*0.53)
        + 0.33*std::sin(t*1.21 + 0.8);
    const double targetRadius = 0.67*strength;
    const Vec3 target{targetRadius*std::cos(targetAngle),
                      targetRadius*std::sin(targetAngle), distance+2.7};
    const Vec3 forward = normalized(target-position);
    const Vec3 levelRight = normalized(cross({0,1,0}, forward));
    const Vec3 levelUp = cross(forward, levelRight);
    const double roll = strength*(0.13*std::sin(t*0.37) + 0.07*std::sin(t*0.83));
    const Vec3 right = levelRight*std::cos(roll) + levelUp*std::sin(roll);
    const Vec3 up = levelUp*std::cos(roll) - levelRight*std::sin(roll);
    return {position, target, forward, right, up};
}

struct PaletteBlend { int from, to; double mix; };
inline PaletteBlend paletteAt(double seconds) {
    constexpr double duration = 14.0, fade = 4.0;
    const double cycle = std::floor(seconds/duration);
    const int from = static_cast<int>(std::fmod(cycle, 6.0));
    const double phase = seconds-cycle*duration;
    return {from, (from+1)%6, smoothStep((phase-(duration-fade))/fade)};
}

struct Resolution { int width=0, height=0; };
inline Resolution parseResolution(const std::string& text) {
    const auto split = text.find_first_of("xX");
    const auto fail = [] { throw std::runtime_error(
        "--resolution: anna koko LEVEYSxKORKEUS (320x240–7680x4320)"); };
    if (split == std::string::npos) fail();
    Resolution r;
    const char* start = text.data();
    const char* end = start+text.size();
    const auto w = std::from_chars(start, start+split, r.width);
    const auto h = std::from_chars(start+split+1, end, r.height);
    if (w.ec != std::errc{} || h.ec != std::errc{} ||
        w.ptr != start+split || h.ptr != end ||
        r.width<320 || r.width>7680 || r.height<240 || r.height>4320) fail();
    return r;
}

} // namespace tunnel
