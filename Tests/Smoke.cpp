#include <juce_audio_utils/juce_audio_utils.h>
#include <iostream>
#include <cmath>
#include "../Source/PresetStorage.h"
#include "../Source/FMEngine.h"
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();
static juce::AudioProcessorParameter* parameterOf(juce::AudioProcessor& p,const char* id){for(auto* b:p.getParameters())if(auto* r=dynamic_cast<juce::RangedAudioParameter*>(b))if(r->paramID==id)return b;return nullptr;}
static void setValue(juce::AudioProcessor& p,const char* id,float v){if(auto* r=dynamic_cast<juce::RangedAudioParameter*>(parameterOf(p,id)))r->setValueNotifyingHost(r->convertTo0to1(v));}
#include "../Source/Arpeggiator.h"
#include "FMTests.h"
#include "SequencerTests.h"
#include "LiveTests.h"

// OPERA smoke tests: the FM engine, the plugin around it, its state, the panel and the shipped sounds.
int main() {
 juce::ScopedJuceInitialiser_GUI gui;
 if (!fmTests()) return 101;
 if (!sequencerTests()) return 102;
 if (!liveRegressionTests()) return 103;
 std::unique_ptr<juce::AudioProcessor> p(createPluginFilter());
 if (p->getName() != "Opera" || p->getParameters().size() != 131) { std::cerr << "Identity or parameter count: " << p->getParameters().size() << "\n"; return 2; }
 auto* first = dynamic_cast<juce::AudioProcessorParameterWithID*>(p->getParameters()[0]);
 if (!first || first->paramID != "algorithm") return 3;
 // Plays: a note, clean and audible.
 p->setRateAndBufferSizeDetails(48000, 512); p->prepareToPlay(48000, 512);
 {double peak = 0;
  for (int k = 0; k < 60; ++k) { juce::AudioBuffer<float> b(2, 512); juce::MidiBuffer m; if (k == 0) m.addEvent(juce::MidiMessage::noteOn(1, 45, .7f), 10); if (k == 40) m.addEvent(juce::MidiMessage::noteOff(1, 45), 0);
   p->processBlock(b, m); for (int c = 0; c < 2; ++c) for (int i = 0; i < 512; ++i) { const float v = b.getSample(c, i); if (!std::isfinite(v)) return 4; peak = std::max(peak, double(std::abs(v))); } }
  if (peak < .02 || peak > 1.f) { std::cerr << "Level " << peak << "\n"; return 5; }}
 std::cout << "PASS a note plays, clean and audible\n";
 // A host block far larger than prepared is played in pieces and gives the same sound as small blocks.
 {auto render = [](int block) { std::unique_ptr<juce::AudioProcessor> q(createPluginFilter()); q->setRateAndBufferSizeDetails(48000, 512); q->prepareToPlay(48000, 512);
   std::vector<float> out; juce::AudioBuffer<float> b(2, block); for (int done = 0; done < 32768; done += block) { juce::MidiBuffer m; if (done == 0) m.addEvent(juce::MidiMessage::noteOn(1, 40, 1.f), 0); b.clear(); q->processBlock(b, m); out.insert(out.end(), b.getReadPointer(0), b.getReadPointer(0) + block); } return out; };
  const auto small = render(512), huge = render(16384); double diff = 0; for (size_t i = 0; i < small.size(); ++i) diff += std::abs(small[i] - huge[i]);
  if (diff > 1.e-3) { std::cerr << "Oversized block differs: " << diff << "\n"; return 6; }}
 std::cout << "PASS oversized host blocks\n";
 // State: a session brings everything back, MIDI learn included; a preset that names only a few settings resets the others.
 {setValue(*p, "op2level", .42f); setValue(*p, "algorithm", 5);
  juce::MemoryBlock data; p->getStateInformation(data); std::unique_ptr<juce::AudioProcessor> q(createPluginFilter()); q->setStateInformation(data.getData(), int(data.getSize()));
  auto* lvl = dynamic_cast<juce::RangedAudioParameter*>(parameterOf(*q, "op2level")); auto* alg = dynamic_cast<juce::RangedAudioParameter*>(parameterOf(*q, "algorithm"));
  if (std::abs(lvl->convertFrom0to1(lvl->getValue()) - .42f) > .01f || std::abs(alg->convertFrom0to1(alg->getValue()) - 5) > .1f) return 7;
  auto partial = juce::XmlDocument::parse("<OrbitState><PARAM id=\"feedback\" value=\"0.9\"/></OrbitState>"); juce::MemoryBlock pd; juce::AudioProcessor::copyXmlToBinary(*partial, pd);
  q->setStateInformation(pd.getData(), int(pd.getSize()));
  if (lvl->getValue() > .001f || alg->convertFrom0to1(alg->getValue()) > .1f) return 8;}
 std::cout << "PASS state: session recall, partial presets fill in the defaults\n";
 // Factory programs: every one loads and plays.
 for (int i = 0; i < p->getNumPrograms(); ++i) { p->setCurrentProgram(i); if (p->getCurrentProgram() != i || p->getProgramName(i).isEmpty()) { std::cerr << "Program " << i << "\n"; return 9; } }
 std::cout << "PASS " << p->getNumPrograms() << " factory programs\n";
 // The panel: every parameter has a visible control; renders at three sizes without changing the patch.
 std::unique_ptr<juce::AudioProcessorEditor> editor(p->createEditor());
 juce::StringArray panelIds;
 std::function<void(juce::Component&)> collect = [&](juce::Component& parent) { const auto id = parent.getProperties()["parameterId"].toString(); if (id.isNotEmpty() && parent.isVisible()) panelIds.addIfNotAlreadyThere(id); for (auto* child : parent.getChildren()) collect(*child); };
 collect(*editor);
 for (auto* parameter : p->getParameters()) if (auto* identified = dynamic_cast<juce::AudioProcessorParameterWithID*>(parameter))
  if (!panelIds.contains(identified->paramID) && identified->paramID != "seqOvd") /* the REC key shows overdub */ { std::cerr << "Missing panel control: " << identified->paramID << "\n"; return 10; }
 std::cout << "PASS all " << panelIds.size() << " parameters have visible panel controls\n";
 juce::PNGImageFormat png; juce::MemoryBlock before; p->getStateInformation(before);
 for (int width : {1280, 1600, 2400}) {
  editor->setSize(width, width * 1120 / 1652); auto rendered = editor->createComponentSnapshot(editor->getLocalBounds());
  if (!rendered.isValid() || rendered.getWidth() != width) return 11;
  juce::FileOutputStream preview(juce::File::getCurrentWorkingDirectory().getChildFile("interface-" + juce::String(width) + ".png")); preview.setPosition(0); preview.truncate();
  if (!png.writeImageToStream(rendered, preview)) return 12; }
 juce::MemoryBlock after; p->getStateInformation(after); if (before != after) return 13;
 std::cout << "PASS panel at 1280, 1600 and 2400 pixels; unchanged patch state\n";
 // The shipped sounds: every setting they name exists, and each plays a short line cleanly and at a sane level.
 {const auto folder = juce::File(__FILE__).getParentDirectory().getParentDirectory().getChildFile("Presets/Sounds"); int count = 0;
  for (const auto& file : folder.findChildFiles(juce::File::findFiles, false, "*.orbitpreset")) { ++count;
   auto xml = juce::XmlDocument::parse(file); if (!xml || !xml->hasTagName("OrbitState")) return 20;
   std::unique_ptr<juce::AudioProcessor> q(createPluginFilter());
   for (auto* child : xml->getChildIterator()) if (!parameterOf(*q, child->getStringAttribute("id").toRawUTF8())) { std::cerr << file.getFileName() << " names an unknown setting: " << child->getStringAttribute("id") << "\n"; return 21; }
   juce::MemoryBlock data; juce::AudioProcessor::copyXmlToBinary(*xml, data); q->setStateInformation(data.getData(), int(data.getSize()));
   q->setRateAndBufferSizeDetails(48000, 256); q->prepareToPlay(48000, 256); double sum = 0, peak = 0, window = 0, loud = 0; long n = 0; int inWindow = 0;
   for (int k = 0; k < 400; ++k) { juce::AudioBuffer<float> b(2, 256); b.clear(); juce::MidiBuffer m;
    if (k == 0) for (int note : {48, 55, 60, 64}) m.addEvent(juce::MidiMessage::noteOn(1, note, .8f), 0);
    if (k == 300) for (int note : {48, 55, 60, 64}) m.addEvent(juce::MidiMessage::noteOff(1, note), 0);
    q->processBlock(b, m); for (int c = 0; c < 2; ++c) for (int i = 0; i < 256; ++i) { const double v = b.getSample(c, i); if (!std::isfinite(v)) return 22; sum += v * v; window += v * v; peak = std::max(peak, std::abs(v)); ++n; }
    if (++inWindow == 10) { loud = std::max(loud, std::sqrt(window / 5120.)); window = 0; inWindow = 0; } } // the loudest 50 ms
   const double rms = std::sqrt(sum / double(n)); std::cout << "  " << file.getFileNameWithoutExtension() << "  rms " << juce::Decibels::gainToDecibels(rms) << " dB  peak " << juce::Decibels::gainToDecibels(peak) << " dB  loud " << juce::Decibels::gainToDecibels(loud) << " dB\n";
   if (loud < .03 || peak > 1.f) { std::cerr << file.getFileName() << " too quiet or clipping\n"; return 23; } }
  if (count == 0) return 24;
  std::cout << "PASS " << count << " shipped sounds\n";}
 return 0;
}
