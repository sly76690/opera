#pragma once
#include <unordered_map>
// A frozen copy of every parameter, read in place of the panel by a second sound (the sequence's own sound in LIVE).
struct ParamLayer {
 using Snapshot=std::array<float,512>;
 void build(AudioProcessorValueTreeState& s,const Array<AudioProcessorParameter*>& all){index.clear();int n=0;
  for(auto* base:all)if(auto* p=dynamic_cast<RangedAudioParameter*>(base))if(auto* a=s.getRawParameterValue(p->paramID)){jassert(n<int(Snapshot{}.size()));index[a]=n++;}
  values.reset(new std::atomic<float>[size_t(jmax(1,n))]);capture();}
 Snapshot panel() const {Snapshot result{};for(const auto& [a,i]:index)result[size_t(i)]=a->load();return result;}
 void capture(){apply(panel());} // one writer: the audio thread after construction
 void assign(Snapshot& data,const std::atomic<float>* a,float value) const {const auto it=index.find(a);if(it!=index.end())data[size_t(it->second)]=value;}
 float get(const std::atomic<float>* a) const {const auto it=index.find(a);return it==index.end()?a->load():values[size_t(it->second)].load();}
 float get(const Snapshot& data,const std::atomic<float>* a) const {const auto it=index.find(a);return it==index.end()?a->load():data[size_t(it->second)];}
 // Restore at a block boundary, never halfway through a voice. No allocation or wait on audio.
 void queue(const Snapshot& data,bool live){const SpinLock::ScopedLockType lock(pendingLock);pending=data;pendingLive=live;hasPending=true;}
 bool applyPending(bool& live){const SpinLock::ScopedTryLockType lock(pendingLock);if(!lock.isLocked()||!hasPending)return false;apply(pending);live=pendingLive;hasPending=false;return true;}
 Snapshot forSave(bool& queued,bool& live) const {
  {const SpinLock::ScopedLockType lock(pendingLock);queued=hasPending;live=pendingLive;if(queued)return pending;}
  // A session save may overlap LIVE activation; retry on the saving thread only.
  Snapshot result{};for(;;){const auto before=revision.load();if(before&1u)continue;
   for(const auto& entry:index)result[size_t(entry.second)]=values[size_t(entry.second)].load();
   if(before==revision.load())return result;}
 }
private:
 void apply(const Snapshot& data){revision.fetch_add(1);for(const auto& entry:index)values[size_t(entry.second)].store(data[size_t(entry.second)]);revision.fetch_add(1);}
 std::unordered_map<const std::atomic<float>*,int> index;std::unique_ptr<std::atomic<float>[]> values;
 std::atomic<unsigned> revision{0};mutable SpinLock pendingLock;Snapshot pending{};bool hasPending=false,pendingLive=false;
};
