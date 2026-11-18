#pragma once
// OPERA: six-operator FM. Each operator is a sine whose phase the operators above it push around (modulation); the
// algorithm says who modulates whom and which operators are heard (carriers). Operator 6 can modulate itself (feedback).
// Each operator has its own envelope (four times, four levels, moving in decibels like the classic FM envelopes),
// its own level, velocity sensitivity and keyboard level scaling. Sixteen voices; principles from public descriptions
// of six-operator FM synthesis. The algorithms and every value here are our own.
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>

namespace opera {

constexpr int operators = 6, voices = 16;

// ---- the sixteen algorithms: for each operator, the operators that modulate it (a bit set) and whether it is heard.
// Modulators always have a higher number than what they modulate, so computing 6 down to 1 needs no delay.
struct Algorithm { std::array<unsigned, operators> from {}; std::array<bool, operators> carrier {}; const char* name; };
inline const std::array<Algorithm, 16>& algorithms() {
 auto make = [](std::initializer_list<std::pair<int, int>> edges, std::initializer_list<int> carriers, const char* name) {
  Algorithm a; a.name = name; for (auto [m, c] : edges) a.from[size_t(c - 1)] |= 1u << (m - 1); for (int c : carriers) a.carrier[size_t(c - 1)] = true; return a; };
 static const std::array<Algorithm, 16> table {{
  make({{2,1},{4,3},{6,5}}, {1,3,5}, "THREE PAIRS"),
  make({{3,2},{2,1},{6,5},{5,4}}, {1,4}, "TWO STACKS OF THREE"),
  make({{6,5},{5,4},{4,3},{3,2},{2,1}}, {1}, "ONE STACK OF SIX"),
  make({{2,1},{6,5},{5,4},{4,3}}, {1,3}, "PAIR AND STACK OF FOUR"),
  make({{2,1},{3,1},{6,5},{5,4}}, {1,4}, "TWO INTO ONE, STACK OF THREE"),
  make({{2,1},{3,1},{4,1},{5,1},{6,1}}, {1}, "FIVE INTO ONE"),
  make({{6,5},{5,4}}, {1,2,3,4}, "STACK OF THREE AND THREE SINES"),
  make({{4,1},{4,2},{4,3},{6,5}}, {1,2,3,5}, "ONE INTO THREE, A PAIR"),
  make({{2,1},{6,5},{5,4},{5,3}}, {1,3,4}, "PAIR AND A BRANCH"),
  make({{6,5},{5,4},{4,3},{3,2}}, {1,2}, "STACK OF FIVE AND A SINE"),
  make({{3,2},{2,1},{6,5},{5,4},{4,1}}, {1}, "TWO STACKS INTO ONE"),
  make({{6,1},{6,2},{6,3},{6,4},{6,5}}, {1,2,3,4,5}, "ONE INTO FIVE"),
  make({{6,5}}, {1,2,3,4,5}, "A PAIR AND FOUR SINES"),
  make({}, {1,2,3,4,5,6}, "SIX SINES"),
  make({{4,3},{6,5}}, {1,2,3,5}, "TWO PAIRS AND TWO SINES"),
  make({{2,1},{3,1},{6,5},{6,4}}, {1,4,5}, "BELL: TWO INTO ONE, ONE INTO TWO"),
 }};
 return table;
}

// ---- settings of one operator and of the whole sound (read once per block) ----
struct OperatorSettings {
 float ratio = 1, fine = 0, detune = 0; // frequency: the note times ratio x (1 + fine), detuned in cents
 float level = 0;       // 0..1 (0 = silent, 1 = full)
 float velocity = .3f;  // 0..1: how much a soft note lowers it
 float keyScale = 0;    // 0..1: how much it fades going up the keyboard from middle C
 std::array<float, 4> time {.005f, .3f, .5f, .3f}; // seconds to reach level 1, 2, 3, then (released) 4
 std::array<float, 4> level4 {1, .8f, .7f, 0};      // envelope levels 1-4, 0..1
};
struct Settings {
 std::array<OperatorSettings, operators> op {};
 int algorithm = 0; float feedback = 0;           // 0..1
 float transpose = 0, bend = 0, tune = 0;          // semitones; tune in cents
 float lfoValue = 0, pitchDepth = 0, ampDepth = 0; // the LFO this sample (-1..1), its depths
 float rateScale = .3f;                            // higher notes, faster envelopes
};

// A sine from a table, interpolated: phase in cycles.
inline float sine(double phase) {
 static const auto table = [] { std::array<float, 4097> t {}; for (size_t i = 0; i < t.size(); ++i) t[i] = float(std::sin(juce::MathConstants<double>::twoPi * double(i) / 4096.)); return t; }();
 const double p = (phase - std::floor(phase)) * 4096.; const int i = int(p); const float f = float(p - i);
 return table[size_t(i)] + (table[size_t(i + 1)] - table[size_t(i)]) * f;
}

// Envelope in decibels: each segment moves the level in dB at a steady pace, so attacks and decays sound like the
// classic FM ones (a fast fall that slows into a long tail). Levels 0..1 map to -96..0 dB.
class Envelope {
public:
 static float toDb(float level) { return level <= 0.f ? -96.f : juce::jlimit(-96.f, 0.f, 20.f * std::log10(level * level)); } // squared: level 0.5 is -12 dB
 void start(const OperatorSettings& s, double sampleRate, float speed) { stage = 0; aim(s, sampleRate, speed); }
 void release(const OperatorSettings& s, double sampleRate, float speed) { stage = 3; aim(s, sampleRate, speed); }
 // Moves the envelope on by count samples (stage changes included) and returns its gain then.
 float advance(int count, const OperatorSettings& s, double sampleRate, float speed) {
  for (int left = count; left > 0;) {
   if (stage >= 4 || stage == 2 && std::abs(db - target) < 1.e-4f) break; // finished, or holding at level 3 while the key is down
   const float stepsToTarget = step == 0 ? 0.f : (target - db) / step;
   if (step == 0 || stepsToTarget <= float(left)) { left -= int(std::ceil(juce::jmax(0.f, stepsToTarget))); db = target;
    if (stage < 2) { ++stage; aim(s, sampleRate, speed); } else if (stage == 3) { stage = 4; break; } else break; }
   else { db += step * float(left); left = 0; }
  }
  return db <= -95.f ? 0.f : std::exp2(db * .16609640f);
 }
 void aim(const OperatorSettings& s, double sampleRate, float speed) {
  target = toDb(s.level4[size_t(stage)]); const float seconds = juce::jmax(.0005f, s.time[size_t(stage)] / speed);
  const float distance = std::abs(target - db), samples = float(seconds * sampleRate);
  // the attack covers its distance in the set time; the falls move at the pace of a full 96 dB in the set time
  const float perSample = stage == 0 ? distance / samples : 96.f / samples;
  step = target >= db ? perSample : -perSample; if (distance < 1.e-4f) step = 0;
 }
 bool done() const { return stage >= 4; }
 void reset() { db = -96.f; stage = 4; step = 0; }
 int stage = 4; float db = -96.f, step = 0, target = -96.f;
};

class Voice {
public:
 void prepare(double rate) { sampleRate = rate; reset(); }
 void reset() { for (auto& e : env) e.reset(); note = -1; held = false; age = 0; fb1 = fb2 = 0; gainNow.fill(0); gainStep.fill(0); }
 bool active() const { for (auto& e : env) if (!e.done()) return true; return false; }
 void start(int midiNote, float vel, const Settings& s, uint64_t order) {
  const bool retrigger = active(); note = midiNote; velocity = vel; held = true; age = order; if (!retrigger) { fb1 = fb2 = 0; for (auto& p : phase) p = 0; }
  const float speed = std::exp2(s.rateScale * float(midiNote - 60) / 12.f);
  for (int i = 0; i < operators; ++i) { if (!retrigger) { env[size_t(i)].db = Envelope::toDb(s.op[size_t(i)].level4[3]); gainNow[size_t(i)] = 0; } env[size_t(i)].start(s.op[size_t(i)], sampleRate, speed); }
  untilGain = 0;
 }
 void stop(const Settings& s) { held = false; const float speed = std::exp2(s.rateScale * float(note - 60) / 12.f); for (int i = 0; i < operators; ++i) env[size_t(i)].release(s.op[size_t(i)], sampleRate, speed); }
 // One sample of this voice.
 float render(const Settings& s) {
  const auto& a = algorithms()[size_t(juce::jlimit(0, 15, s.algorithm))];
  const float speed = std::exp2(s.rateScale * float(note - 60) / 12.f);
  const double base = 440. * std::exp2((double(note) - 69. + s.transpose + s.bend + s.tune / 100. + s.lfoValue * s.pitchDepth) / 12.);
  std::array<float, operators> out {}; float sum = 0; int heard = 0;
  if (--untilGain <= 0) { untilGain = gainBlock; // envelopes: computed every 16 samples, joined by straight lines
   for (int i = 0; i < operators; ++i) { const float target = env[size_t(i)].advance(gainBlock, s.op[size_t(i)], sampleRate, speed) * gain(s.op[size_t(i)]);
    gainStep[size_t(i)] = (target - gainNow[size_t(i)]) / float(gainBlock); } }
  for (int i = operators - 1; i >= 0; --i) {
   const auto& o = s.op[size_t(i)];
   gainNow[size_t(i)] += gainStep[size_t(i)]; const float level = gainNow[size_t(i)];
   float pm = 0; for (int m = 0; m < operators; ++m) if (a.from[size_t(i)] & (1u << m)) pm += out[size_t(m)];
   if (i == operators - 1) pm += s.feedback * .35f * (fb1 + fb2); // operator 6 hears itself
   const double hz = base * double(o.ratio) * (1. + double(o.fine)) * std::exp2(double(o.detune) / 1200.);
   phase[size_t(i)] += hz / sampleRate; if (phase[size_t(i)] >= 1.) phase[size_t(i)] -= std::floor(phase[size_t(i)]);
   const float y = sine(phase[size_t(i)] + double(pm)) * level;
   if (i == operators - 1) { fb2 = fb1; fb1 = y; }
   if (a.carrier[size_t(i)]) { sum += y; ++heard; }
   out[size_t(i)] = y * 1.6f; // as a modulator, in cycles: a full-level operator moves the next one's phase by 1.6 cycles (about ten radians)
  }
  sum *= 1.f - s.ampDepth * .5f * (1.f + s.lfoValue);
  return heard > 0 ? sum / std::sqrt(float(heard)) : 0.f;
 }
 int note = -1; bool held = false; uint64_t age = 0;
private:
 float gain(const OperatorSettings& o) const {
  const float vel = 1.f - o.velocity * (1.f - velocity);
  const float keyDb = -o.keyScale * 18.f * juce::jmax(0.f, float(note - 60)) / 24.f;
  return o.level * o.level * vel * std::pow(10.f, keyDb / 20.f);
 }
 static constexpr int gainBlock = 16;
 double sampleRate = 96000; std::array<double, operators> phase {}; std::array<Envelope, operators> env {}; float velocity = 1, fb1 = 0, fb2 = 0;
 std::array<float, operators> gainNow {}, gainStep {}; int untilGain = 0;
};

// Sixteen voices: a new note takes a free voice, else the oldest released one, else the oldest. A note played again
// reuses its own voice. The sustain pedal holds released notes.
class Synth {
public:
 void prepare(double rate) { for (auto& v : voiceList) v.prepare(rate); sustained.fill(false); pedal = false; }
 void reset() { for (auto& v : voiceList) v.reset(); sustained.fill(false); pedal = false; }
 void noteOn(int note, float vel, const Settings& s) {
  Voice* chosen = nullptr; for (auto& v : voiceList) if (v.note == note && v.active()) { chosen = &v; break; }
  if (!chosen) for (auto& v : voiceList) if (!v.active()) { chosen = &v; break; }
  if (!chosen) for (auto& v : voiceList) if (!v.held && (!chosen || v.age < chosen->age)) chosen = &v;
  if (!chosen) for (auto& v : voiceList) if (!chosen || v.age < chosen->age) chosen = &v;
  sustained[size_t(note & 127)] = false; chosen->start(note, vel, s, ++order);
 }
 void noteOff(int note, const Settings& s) { if (pedal) { sustained[size_t(note & 127)] = true; return; } for (auto& v : voiceList) if (v.held && v.note == note) v.stop(s); }
 void sustain(bool down, const Settings& s) { pedal = down; if (!down) for (int n = 0; n < 128; ++n) if (sustained[size_t(n)]) { sustained[size_t(n)] = false; noteOff(n, s); } }
 void allOff(const Settings& s) { sustained.fill(false); pedal = false; for (auto& v : voiceList) if (v.held) v.stop(s); }
 float render(const Settings& s) { float sum = 0; for (auto& v : voiceList) if (v.note >= 0 && v.active()) sum += v.render(s); return sum; }
 int sounding() const { int n = 0; for (auto& v : voiceList) if (v.note >= 0 && v.active()) ++n; return n; }
private:
 std::array<Voice, voices> voiceList {}; std::array<bool, 128> sustained {}; bool pedal = false; uint64_t order = 0;
};

} // namespace opera
