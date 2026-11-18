#pragma once
#include <juce_core/juce_core.h>
#include <FactoryData.h>

// OPERA factory bank: the factory sounds (Presets/Sounds) embedded in the binary, played by BANK and PATCH 1-8 in the
// order below, so the program buttons give Opera's own sounds even before the installer ran.
class FactoryBank {
public:
 static const juce::StringArray& order() {
  static const juce::StringArray names{
   // A: keys
   "EPIANO - Tine Glow","EPIANO - Soft Bark","EPIANO - Glass Keys","EPIANO - Bright Tines",
   "KEYS - Clav Snap","KEYS - Harpsi Wire","KEYS - Vibes Air","KEYS - Toy Piano",
   // B: bells and mallets
   "BELL - Glass Tower","BELL - Tubular","BELL - Music Box","BELL - Temple Gong",
   "BELL - Ice Chimes","MALLET - Marimba","MALLET - Kalimba","MALLET - Steel Drum",
   // C: basses
   "BASS - Rubber FM","BASS - Solid Slap","BASS - Sub Sine","BASS - Wood Upright",
   "BASS - Growl Stack","BASS - Metal Pluck","BASS - Hollow Pulse","BASS - Funk Pick",
   // D: brass, leads, strings and plucks
   "BRASS - Gold Section","BRASS - Soft Horns","BRASS - Stabs","LEAD - Reed Solo",
   "LEAD - Saw Stack","LEAD - Flute Breath","STRINGS - FM Ensemble","KEYS - Nylon Pluck",
   // E: organs, pads and effects
   "ORGAN - Drawbars","ORGAN - Perc Organ","PAD - Crystal Air","PAD - Glass Choir",
   "PAD - Slow Evolve","FX - Laser Drop","FX - Metal Rain","FX - Wind Noise"};
  return names;
 }
 static std::unique_ptr<juce::XmlElement> load(const juce::String& name) {
  for(int i=0;i<FactoryData::namedResourceListSize;++i)
   if(juce::String::fromUTF8(FactoryData::originalFilenames[i]).upToLastOccurrenceOf(".orbitpreset",false,false)==name){
    int size=0;const auto* data=FactoryData::getNamedResource(FactoryData::namedResourceList[i],size);
    return data?juce::XmlDocument::parse(juce::String::fromUTF8(data,size)):nullptr;}
  return {};
 }
};
