#pragma once
#include <juce_core/juce_core.h>
#include <FactoryData.h>

// OPERA factory bank: the factory sounds (Presets/Sounds) embedded in the binary, played by BANK and PATCH 1-8 in the
// order below, so the program buttons give Opera's own sounds even before the installer ran.
class FactoryBank {
public:
 static const juce::StringArray& order() {
  static const juce::StringArray names{
   // A: electric pianos and keys
   "EP - Tine Classic","EP - Ballad Tines","EP - Bark Stage","EP - Bell Tines",
   "KEYS - FM Piano","KEYS - Snap Clav","KEYS - Harpsichord","KEYS - Toy Piano",
   // B: basses
   "BASS - Solid Slap","BASS - Synth Punch","BASS - Rubber Drop","BASS - Wood Upright",
   "BASS - Sub Sine","BASS - Metal Pick","BASS - Funk Finger","BASS - Squelch FM",
   // C: brass and leads
   "BRASS - Synth Section","BRASS - Soft Horns","BRASS - Stab 84","BRASS - Power Swell",
   "LEAD - Hard Edge","LEAD - Reed Solo","LEAD - Flute Breath","LEAD - Square Solo",
   // D: bells, mallets and plucks
   "BELL - Tubular","BELL - Glass Tower","BELL - Music Box","MALLET - Marimba",
   "MALLET - Vibes","MALLET - Kalimba","PLUCK - Koto","PLUCK - Nylon Guitar",
   // E: strings, pads, voices, organs, percussion and effects
   "STRINGS - FM Ensemble","PAD - Glass Choir","PAD - Warm Haze","VOICE - Ah Choir",
   "ORGAN - Drawbars","ORGAN - Perc Organ","PERC - FM Toms","FX - Laser Zap"};
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
